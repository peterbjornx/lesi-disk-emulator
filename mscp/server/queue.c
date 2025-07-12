#include "mscp/server/server.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>

void mscps_enqueue_cmd( mscps_t *server, mscpc_t *cmd ) {
    if ( server->cq_tail ) {
        server->cq_tail->next = cmd;
    } else {
        server->cq_head = cmd;
    }
    cmd->next = NULL;
    server->cq_tail = cmd;
    server->cq_count++;
}

void mscps_send_response( mscps_t *server, void *end, int conn, int sz, int type ) {
    mscpc_t *endw = malloc(sizeof(mscpc_t));
    endw->data = end;
    endw->data_len = endw->msg_len = sz;
    endw->conn_id  = conn;
    endw->credit   = 3; // ?
    endw->msg_type = type;
    //TODO: ordering
    if ( server->rq_tail ) {
        server->rq_tail->next = endw;
    } else {
        server->rq_head = endw;
    }
    endw->next = NULL;
    server->rq_tail = endw;
    server->rq_count++;
}

void mscps_send_end( mscps_t *server, mscpc_t *pkt ) {
    pkt->credit   = 1; // ?
    pkt->resp->m_endcode |= M_OP_END;
    if ( pkt->msg_len == 0 )
        pkt->msg_len = 60;
    //TODO: ordering
    if ( server->rq_tail ) {
        server->rq_tail->next = pkt;
    } else {
        server->rq_head = pkt;
    }
    pkt->next = NULL;
    server->rq_tail = pkt;
    server->rq_count++;
}

int mscps_send_rq( mscps_t *server ) {
    mscpc_t *resp, *next;
    int status;

    resp = server->rq_head;

    while ( resp != NULL ) {
        next = resp->next;
        status = hostif_send_response( server->hostif, resp );

        if ( status == ERR_BUSY )
            return ERR_OK;
        else if ( status != ERR_OK )
            return status;
        server->rq_count--;
        resp->next = NULL;
        server->rq_head = next;

        if ( resp == server->rq_tail )
            server->rq_tail = NULL;

        resp = next;
    }
    return ERR_OK;
}