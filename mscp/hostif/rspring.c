#include "mscp/hostif/hostif.h"
#include "lesi/lesi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int dbg_respring = 0;

int hostif_rring_do( mscpa_t *a ) {
    int status, idx, want_irq;
    uint32_t descptr;
    uint32_t envptr;
    mscpc_t *pkt;

    idx     = a->rring_idx;
    descptr = a->rring_base + idx * sizeof(hostif_desc_t);
    pkt     = a->rring_pkt;

    switch ( a->rring_state ) {
        case CS_UNUSED:
            return ERR_OK;
        case CS_WAITFULL:
            assert( pkt != NULL );

            if ( dbg_respring )
                printf("MSCP R: polling slot %i\n", idx);
            
            status = lesi_set_host_addr( descptr );
            propagateTagged( status, WHEN_KLESI_CMD );

            status = lesi_read_dma( (uint16_t *) &a->rring_desc, sizeof(hostif_desc_t) / 2 );
            status = hostif_ringxfer_err( a, status, FATAL_RING_READ );
            propagateTagged( status, WHEN_CTRL_READ );

            if ( ~a->rring_desc & MSCP_DESC_OWNER ) {
                /* We don't own this descriptor. That means for now no */
                /* more descriptors are available. */
                if ( dbg_respring )
                    printf("MSCP R: response ring full, can't send response\n");
                return ERR_OK;
            }

            /* Checkpoint */ 
            a->rring_state = CS_XFERSZ;
            if ( dbg_respring )
                printf("MSCP R: Accepted response descriptor [%3i] Addr: %09o F: %i\n",
                idx, a->rring_desc & a->addrmask,
                (a->rring_desc & MSCP_DESC_FLAG) != 0 );
            
            a->r_fir = a->rring_desc & MSCP_DESC_FLAG;

        case CS_XFERSZ:
            /* --------------- Transfer response envelope in ----------------- */
            envptr = (a->rring_desc & a->addrmask) - sizeof(hostif_envhdr_t);
            if ( pkt->msg_len > 60 ) {
                status = lesi_set_host_addr( envptr );
                propagateTagged( status, WHEN_KLESI_CMD );

                status = lesi_read_dma( (uint16_t *) &a->rring_hdr, sizeof(hostif_envhdr_t) / 2 );
                status = hostif_ringxfer_err( a, status, FATAL_ENV_PKT_READ );
                propagateTagged( status, WHEN_CTRL_READ );

                //TODO: checkpoint here?
                a->rring_bufsz = a->rring_hdr.msg_len;
            } else
                a->rring_bufsz = pkt->msg_len;

            //TODO: Figure out how to deal with fragmentation
            a->rring_hdr.conn_id = pkt->conn_id;
            a->rring_hdr.msg_len = pkt->msg_len;
            a->rring_hdr.type_credits = (pkt->msg_type << 4)  | pkt->credit;
            
            if ( pkt->msg_len > a->rring_bufsz ) {
                a->rring_hdr.msg_len = a->rring_bufsz;
                
            }

            a->rring_state = CS_XFER_PAYL;

        case CS_XFER_PAYL:
            /* --------------- Transfer response payload ----------------- */

            /* Resumed */
            status = lesi_set_host_addr( a->rring_desc & a->addrmask );
            propagateTagged( status, WHEN_KLESI_CMD );
            
            //TODO: Combine with header read as size is always >= 64
            //TODO: Honor burst limit

            status = lesi_write_dma( pkt->data, pkt->msg_len / 2);
            status = hostif_ringxfer_err( a, status, FATAL_ENV_PKT_WRITE );
            propagateTagged( status, WHEN_CTRL_WRITE );
            
            if ( dbg_respring )
                printf("MSCP R: Transfered response payload [%3i]\n", idx);

            a->rring_state = CS_XFER_ENV;

        case CS_XFER_ENV:

            /* --------------- Transfer response envelope ----------------- */
            envptr = (a->rring_desc & a->addrmask) - sizeof(hostif_envhdr_t);

            status = lesi_set_host_addr( envptr );
            propagateTagged( status, WHEN_KLESI_CMD );

            status = lesi_write_dma( (uint16_t *) &a->rring_hdr, sizeof(hostif_envhdr_t) / 2 );
            status = hostif_ringxfer_err( a, status, FATAL_ENV_PKT_WRITE );
            propagateTagged( status, WHEN_CTRL_WRITE );

            if ( dbg_respring )
                printf("MSCP R: Sent response envelope [%3i] Conn ID: %i Type: %i, Credits: %i, Length: %i\n",
                    idx, a->rring_hdr.conn_id, pkt->msg_type, pkt->credit, a->rring_hdr.msg_len );

            a->rring_state = CS_XFER_OWN;

        case CS_XFER_OWN:
            /* --------------- Transfer command ownership ----------------- */

            a->rring_desc |=  MSCP_DESC_FLAG;
            a->rring_desc &= ~MSCP_DESC_OWNER;

            status = lesi_set_host_addr( descptr );
            propagateTagged( status, WHEN_KLESI_CMD );

            status = lesi_write_dma( (uint16_t *) &a->rring_desc, sizeof(hostif_desc_t) / 2 );
            status = hostif_ringxfer_err( a, status, FATAL_RING_WRITE );
            propagateTagged( status, WHEN_CTRL_WRITE );

            if ( ~a->r_fir & MSCP_DESC_FLAG ) {
                break; /* done */
            }

            a->rring_state = CS_SENDIRQ;

        case CS_SENDIRQ:

            if ( a->rsize == 1 ) {
                want_irq = 1;
            } else {
                /* Read previous descriptor to see if the ring was empty */
                descptr = a->rring_base + ((idx - 1) & (a->rsize - 1)) * sizeof(hostif_desc_t);

                status = lesi_set_host_addr( descptr );
                propagateTagged( status, WHEN_KLESI_CMD );

                status = lesi_read_dma( (uint16_t *) &a->rring_desc, sizeof(hostif_desc_t) / 2 );
                status = hostif_ringxfer_err( a, status, FATAL_RING_READ );
                propagateTagged( status, WHEN_CTRL_READ );

                want_irq = a->rring_desc & MSCP_DESC_OWNER;
            }

            if ( want_irq ) {
                status = hostif_send_ring_irq( a, 0, 1 );
                propagate( status );
            }

            
            if ( dbg_respring )
                printf("MSCP R: Transfered response ownership [%3i]\n", idx);
            
            break; /* done */
    }

    a->rring_idx = (idx + 1) & (a->rsize - 1);
    free( a->rring_pkt->data );
    free( a->rring_pkt );
    a->rring_pkt = NULL;
    a->rring_state = CS_UNUSED;
    return ERR_OK;

}

int hostif_send_response(  mscpa_t *a, mscpc_t *resp ) {
    int status;

    if ( a->rring_state != CS_UNUSED )
        return ERR_BUSY;
    
    a->rring_state = CS_WAITFULL;
    a->rring_pkt   = resp;
 
    return ERR_OK;
}

void hostif_rring_reset( mscpa_t *a ) {
    if ( a->rring_pkt ) {
        if ( a->rring_pkt->data )
            free( a->rring_pkt->data );
        free( a->rring_pkt );
    }
    a->rring_idx = 0;
    a->rring_state = CS_UNUSED;

}