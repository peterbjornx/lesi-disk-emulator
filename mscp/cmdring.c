#include "mscp/mscp.h"
#include "lesi/lesi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "projconfig.h"
#include "mscp/packet.h"

int mscp_fetch_cring( mscpa_t *a, mscpc_t **target ) {
    int status, idx, want_irq;
    uint32_t descptr;
    uint32_t envptr;
    mscp_desc_t desc;
    int tstate = CS_UNUSED;

    if ( *target != NULL ) {
        desc   = (*target)->desc;
        tstate = (*target)->state;
    }

    idx = a->cring_idx;
    descptr = a->cring_base + idx * sizeof(mscp_desc_t);

    switch ( tstate ) {
        case CS_UNUSED:
            printf("MSCP C: polling slot %i %p %p\n",idx, target, *target);
            status = lesi_set_host_addr( descptr );
            if ( status )
                return status;

            status = lesi_read_dma( (uint16_t *) &desc, sizeof(mscp_desc_t) / 2 );
            if ( status )
                return status;

            if ( ~desc & MSCP_DESC_OWNER ) {
                *target = NULL;
                a->c_poll = 0;
                    printf("MSCP C: command ring empty, halting polling\n");
                return ERR_OK;
            }

            *target = malloc( sizeof(mscpc_t) );
            if ( *target == NULL ) {
                printf("MSCP: Could not malloc command queue entry\n");
                return ERR_OK; //TODO: Do we want this to be an error?
            }
            printf("alloced %p\n", *target );
            memset( *target, 0, sizeof(mscpc_t) );

            (*target)->desc = desc;
            (*target)->state = CS_XFER_ENV;
            printf("MSCP C: Accepted command descriptor [%3i] Addr: %09o F: %i\n",
                idx, desc & a->addrmask,
                (desc & MSCP_DESC_FLAG) != 0 );

            
            a->c_fir = (*target)->desc & MSCP_DESC_FLAG;

    case CS_XFER_ENV:
            /* --------------- Transfer command envelope ----------------- */
            envptr = (desc & a->addrmask) - sizeof(mscp_envhdr_t);
            status = lesi_set_host_addr( envptr );
            if ( status )
                return status;

            status = lesi_read_dma( (uint16_t *) &(*target)->hdr, sizeof(mscp_envhdr_t) / 2 );
            if ( status ) 
                return status;

            (*target)->state    = CS_XFER_PAYL;
            (*target)->msg_type = ((*target)->hdr.type_credits >> 4) & 0xF;
            (*target)->credit   = ((*target)->hdr.type_credits     ) & 0xF;
            (*target)->data_len = (((*target)->hdr.msg_len + 1) / 2);
            if ( (*target)->data_len < 30 )
                (*target)->data_len = 30;
            (*target)->data_len *= 2;

            printf("MSCP C: Accepted command envelope [%3i] Conn ID: %i Type: %i, Credits: %i, Length: %i\n",
                idx, (*target)->hdr.conn_id, (*target)->msg_type, (*target)->credit, (*target)->hdr.msg_len );
    case CS_XFER_PAYL:
            /* --------------- Transfer command payload ----------------- */
            (*target)->data = malloc( (*target)->data_len );
            if ( (*target)->data == NULL ) {
                printf("MSCP C: Could not malloc command payload\n");
                return ERR_OK; //TODO: Do we want this to be an error?
            }

            /* Resumed */
            status = lesi_set_host_addr( desc & a->addrmask );
            if ( status ){
                free( (*target)->data );
                return status;
            }
            
            //TODO: Combine with header read as size is always >= 64
            //TODO: Honor burst limit

            status = lesi_read_dma( (*target)->data, (*target)->data_len  / 2);
            if ( status ) {
                free( (*target)->data );
                return status;
            }
            
            printf("MSCP C: Transfered command payload [%3i]\n", idx);

            (*target)->state = CS_XFER_OWN;
    case CS_XFER_OWN:
            /* --------------- Transfer command ownership ----------------- */

            (*target)->desc |= MSCP_DESC_FLAG;
            (*target)->desc &= ~MSCP_DESC_OWNER;

            status = lesi_set_host_addr( descptr );
            if ( status )
                return status;

            status = lesi_write_dma( (uint16_t *) &(*target)->desc, sizeof(mscp_desc_t) / 2 );
            if ( status ) 
                return status;

            if ( ~a->c_fir & MSCP_DESC_FLAG ) {
                a->cring_idx = (idx + 1) & (a->csize - 1);
                (*target)->state = CS_QUEUED;
                return ERR_OK;
            }

            (*target)->state = CS_SENDIRQ;

    case CS_SENDIRQ:

            if ( a->csize == 1 ) {
                want_irq = 1;
            } else {
                /* Read previous descriptor to see if the ring is full */
                descptr = a->cring_base + ((idx - 1) & (a->csize - 1)) * sizeof(mscp_desc_t);

                status = lesi_set_host_addr( descptr );
                if ( status )
                    return status; 

                status = lesi_read_dma( (uint16_t *) &desc, sizeof(mscp_desc_t) / 2 );
                if ( status )
                    return status;

                want_irq = desc & MSCP_DESC_OWNER;
            }

            if ( want_irq )
                mscp_send_ring_irq( a );


            
            printf("MSCP C: Transfered command ownership [%3i]\n", idx);
            a->cring_idx = (idx + 1) & (a->csize - 1);

            (*target)->state = CS_QUEUED;
            return ERR_OK;
    }

}

int mscp_poll_cring(  mscpa_t *a ) {
    int status;
    mscpc_t **curnext = &a->cmd_queue;
    for ( ;; ) {
        if ( *curnext == NULL || (*curnext)->state != CS_QUEUED ) {
            status = mscp_fetch_cring( a, curnext );
            if ( status )
                return status;
        }
        if ( *curnext == NULL )
            break;
        curnext = &(*curnext)->next;
    }
    return ERR_OK;
}