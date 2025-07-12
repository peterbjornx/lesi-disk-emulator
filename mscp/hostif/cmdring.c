#include "mscp/hostif/hostif.h"
#include "lesi/lesi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "projconfig.h"
#include "mscp/packet.h"

int dbg_cmdring = 0;

int hostif_ringxfer_err( mscpa_t *a, int status, int fcode ) {
    int err = ERR_STATUS( status );
    if ( err == ERR_NXM )
        a->fatal_code = FATAL_BUSMASTER_ERR;
    if ( err == ERR_INIT || err == 0 )
        return status;
    else
        a->fatal_code = fcode;
    return status | ERR_FATAL;
}

int hostif_cring_fetch( mscpa_t *a ) {
    int status, idx, want_irq;
    mscpc_t *pkt;
    uint32_t descptr;
    uint32_t envptr;

    idx = a->cring_idx;
    descptr = a->cring_base + idx * sizeof(hostif_desc_t);
    pkt = a->cring_pkt;

    switch ( a->cring_state ) {
        case CS_UNUSED:
            if ( dbg_cmdring )
                printf("MSCP C: polling slot %i\n", idx);
            
            status = lesi_set_host_addr( descptr );
            propagateTagged( status, WHEN_KLESI_CMD );

            status = lesi_read_dma( (uint16_t *) &a->cring_desc, sizeof(hostif_desc_t) / 2 );
            status = hostif_ringxfer_err( a, status, FATAL_RING_READ );
            propagateTagged( status, WHEN_CTRL_READ );
            

            if ( ~a->cring_desc & MSCP_DESC_OWNER ) {
                /* We don't own this descriptor. That means for now no */
                /* more descriptors are available. */
                a->cring_pkt = NULL;
                a->c_poll = 0;
                if ( dbg_cmdring )
                    printf("MSCP C: command ring empty, halting polling\n");
                return ERR_OK;
            }

            /* Allocate and zero packet */
            pkt = a->cring_pkt = malloc( sizeof(mscpc_t) );
            if ( a->cring_pkt == NULL ) {
                printf("MSCP: Could not malloc command queue entry\n");
                return ERR_OK; //TODO: Do we want this to be an error?
            }

            memset( pkt, 0, sizeof(mscpc_t) );


            /* Checkpoint */ 
            a->cring_state = CS_XFER_ENV;
            if ( dbg_cmdring )
                printf("MSCP C: Accepted command descriptor [%3i] Addr: %09o F: %i\n",
                idx, a->cring_desc & a->addrmask,
                (a->cring_desc & MSCP_DESC_FLAG) != 0 );
            
            a->c_fir = a->cring_desc & MSCP_DESC_FLAG;

    case CS_XFER_ENV:
            /* --------------- Transfer command envelope ----------------- */
            envptr = (a->cring_desc & a->addrmask) - sizeof(hostif_envhdr_t);
            status = lesi_set_host_addr( envptr );
            propagateTagged( status, WHEN_KLESI_CMD );

            status = lesi_read_dma( (uint16_t *) &a->cring_hdr, sizeof(hostif_envhdr_t) / 2 );
            status = hostif_ringxfer_err( a, status, FATAL_ENV_PKT_READ );
            propagateTagged( status, WHEN_CTRL_READ );

            a->cring_state      = CS_XFER_PAYL;
            pkt->msg_type = (a->cring_hdr.type_credits >> 4) & 0xF;
            pkt->credit   = (a->cring_hdr.type_credits     ) & 0xF;
            pkt->data_len = ((a->cring_hdr.msg_len + 1) / 2);
            pkt->conn_id  = a->cring_hdr.conn_id;
            pkt->msg_len  = a->cring_hdr.msg_len;

            if ( pkt->data_len < 30 )
                pkt->data_len = 30;
            pkt->data_len *= 2;

            if ( dbg_cmdring )
                printf("MSCP C: Accepted command envelope [%3i] Conn ID: %i Type: %i, Credits: %i, Length: %i\n",
                    idx, a->cring_hdr.conn_id, pkt->msg_type, pkt->credit, a->cring_hdr.msg_len );
    case CS_XFER_PAYL:
            /* --------------- Transfer command payload ----------------- */
            pkt->data = malloc( pkt->data_len );
            if ( pkt->data == NULL ) {
                printf("MSCP C: Could not malloc command payload\n");
                return ERR_OK; //TODO: Do we want this to be an error?
            }

            /* Resumed */
            status = lesi_set_host_addr( a->cring_desc & a->addrmask );
            if ( status ) {
                free( pkt->data );
                propagateTagged( status, WHEN_KLESI_CMD );
            }
            
            //TODO: Combine with header read as size is always >= 64
            //TODO: Honor burst limit

            status = lesi_read_dma( pkt->data, pkt->data_len  / 2);
            status = hostif_ringxfer_err( a, status, FATAL_ENV_PKT_READ );
            if ( status ) {
                free( pkt->data );
                propagateTagged( status, WHEN_CTRL_READ );
            }
            
            if ( dbg_cmdring )
                printf("MSCP C: Transfered command payload [%3i]\n", idx);

            a->cring_state = CS_XFER_OWN;
    case CS_XFER_OWN:
            /* --------------- Transfer command ownership ----------------- */

            a->cring_desc |=  MSCP_DESC_FLAG;
            a->cring_desc &= ~MSCP_DESC_OWNER;

            status = lesi_set_host_addr( descptr );
            propagateTagged( status, WHEN_KLESI_CMD );

            status = lesi_write_dma( (uint16_t *) &a->cring_desc, sizeof(hostif_desc_t) / 2 );
            status = hostif_ringxfer_err( a, status, FATAL_RING_WRITE );
            propagateTagged( status, WHEN_CTRL_WRITE );

            if ( ~a->c_fir & MSCP_DESC_FLAG ) {
                break; /* done */
            }

            a->cring_state = CS_SENDIRQ;

    case CS_SENDIRQ:

            if ( a->csize == 1 ) {
                want_irq = 1;
            } else {
                /* Read previous descriptor to see if the ring is full */
                descptr = a->cring_base + ((idx - 1) & (a->csize - 1)) * sizeof(hostif_desc_t);

                status = lesi_set_host_addr( descptr );
                propagateTagged( status, WHEN_KLESI_CMD );

                status = lesi_read_dma( (uint16_t *) &a->cring_desc, sizeof(hostif_desc_t) / 2 );
                status = hostif_ringxfer_err( a, status, FATAL_RING_READ );
                propagateTagged( status, WHEN_CTRL_READ );

                want_irq = a->cring_desc & MSCP_DESC_OWNER;
            }

            if ( want_irq ) {
                status = hostif_send_ring_irq( a, 1, 0 );
                propagate( status );
            }
            
            if ( dbg_cmdring )
                printf("MSCP C: Transfered command ownership [%3i]\n", idx);
            
            break; /* done */
    }

    a->cring_idx = (idx + 1) & (a->csize - 1);
    a->cring_state = CS_QUEUED;
    return ERR_OK;

}

int hostif_handle_poll( mscpa_t *a ) {
    int status;
    uint16_t sr;

    status = lesi_read_srflags( &sr );
    propagateTagged( status, WHEN_KLESI_CMD );
    
    if ( ~sr & LESI_SR_POLL )
        return ERR_OK;
    
    /* Clear the poll flag */
    status = lesi_read_reg( LESI_REG_CLEAR_POLL, &sr );
    lesi_lowlevel_read_strobe(0);
    if (dbg_cmdring)
        printf("MSCP C: Got POLL, resuming polling.\n");
    a->c_poll = 1;

    propagateTagged( status, WHEN_KLESI_CMD );

    return ERR_OK;
}

int hostif_cring_poll( mscpa_t *a ) {
    int status;

    if ( !a->c_poll ) {
        return hostif_handle_poll( a );
    }

    for ( ;; ) {
        status = hostif_cring_fetch( a );
        propagate( status );

        if ( a->cring_state != CS_QUEUED )
            break;

        mscps_enqueue_cmd( a->server, a->cring_pkt );
        //TODO: Throttle if queue oversize?
        a->cring_pkt   = NULL;
        a->cring_state = CS_UNUSED;
    }

    return ERR_OK;
 
}

void hostif_cring_reset( mscpa_t *a ) {
    if ( a->cring_pkt ) {
        if ( a->cring_pkt->data )
            free( a->cring_pkt->data );
        free( a->cring_pkt );
    }
    a->cring_idx = 0;
    a->cring_state = CS_UNUSED;

}