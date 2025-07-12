#include "mscp/server/server.h"
#include "mscp/hostif/hostif.h"
#include "projconfig.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void hexdump( void *a ,int count ) {
    uint8_t *h=  a;
    for ( int i = 0; i < count; i++ ) {
        if ( (i & 0xF) == 0x0 )
            printf("%04X: ", i);
        printf("%02X ",h[i]);
        if ( (i & 0xF) == 0xf )
            printf("\n");
    }
    printf("\n");
}

int mscp_run_command( mscps_t *server, mscpc_t *cmd ) {
    int typ = cmd->msg_type;
    int sz = 32;
    int status;
    mscp_resp_t *end = malloc(sizeof(mscp_resp_t));
    memset( end, 0, sizeof(mscp_resp_t));
    mscp_pkt_t *pkt = (void *)cmd->data;
    mscpu_t *unit = NULL;
    end->m_cmdref = pkt->m_cmdref;
    end->m_seqn   = 0; // ?
    end->m_endcode = pkt->m_opcode | M_OP_END;
    end->m_status  = M_ST_ICMD;
    if ( pkt->m_unit >= 0 && pkt->m_unit < server->c_numunits ) {
        unit = server->c_unit + pkt->m_unit;
    } else {
        printf("unit offline: %i\n", pkt->m_unit);
        end->m_status = M_ST_OFFLN; //TOOD: is this right?
        goto reply;
    }
    //printf("Got packet: opcode = %i\n", pkt->m_opcode );
    switch( pkt->m_opcode ) {
        case M_OP_STCON: status = mscp_cntrl_scc   ( server, pkt, end, &sz ); break;
        case M_OP_ONLIN: status = mscpu_online     ( unit  , pkt, end, &sz ); break;
        case M_OP_STUNT: status = mscpu_setchar    ( unit  , pkt, end, &sz ); break;
        case M_OP_ACCES:
        case M_OP_WRITE:
        case M_OP_COMP :
        case M_OP_ERASE:
        case M_OP_READ : 
            free( end );
            status = mscpu_enqueue( unit, cmd ); return 1;
        default:
            printf("Got unknown packet: opcode = %i\n", pkt->m_opcode );
            hexdump(cmd->data, cmd->data_len);
            break;
    }
reply:
    mscps_send_response(server, end, cmd->conn_id, sz, typ);
    mscps_cmd_free( cmd );
    return 1;
}

void mscps_cmd_free( mscpc_t *cmd ) {
    free( cmd->data );
    free( cmd );
}

void mscps_cmd_handle( mscps_t *server ) {
    mscpc_t *cmd, *next;

    cmd = server->cq_head;

    while ( cmd != NULL ) {
        next = cmd->next;
        server->cq_head = next;
        if ( cmd == server->cq_tail )
            server->cq_tail = NULL;
        mscp_run_command( server, cmd );
        cmd = next;
    }
}

void mscps_reinit( mscps_t *server ) {
    int i;
    //TODO: Server reinit
    for ( i = 0; i < server->c_numunits; i++ )
        mscpu_reinit( server->c_unit + i ); //TODO: Handle errors
}

void mscps_loop( mscps_t *server ) {
    int i;
    mscps_cmd_handle( server );
    mscps_send_rq( server );
    for ( i = 0; i < server->c_numunits; i++ )
        mscpu_process( server->c_unit + i ); //TODO: Handle errors

}

void mscps_attach( mscps_t *server ,mscpa_t *hostif ) {
    hostif_set_server( hostif, server );
    server->hostif = hostif;
}

mscps_t *mscps_setup( void ) {
    mscps_t *server;
    int i;

    server = malloc( sizeof(mscps_t) );
    if ( server == NULL )
        return NULL;
    
    memset( server, 0, sizeof(mscps_t) );

    server->c_unit = malloc( MSCP_CUNITS * sizeof( mscpu_t ) );
    if ( server->c_unit == NULL ) {
        free( server );
        return NULL;
    }

    memset( server->c_unit, 0, MSCP_CUNITS * sizeof( mscpu_t ) );

    server->cq_head  = server->cq_tail = NULL;
    server->cq_count = 0;

    server->c_numunits   = MSCP_CUNITS;
    server->c_id.i_class = MSCP_CID_CLASS;
    server->c_id.i_model = MSCP_CID_MODEL;
    server->c_id.i_uid_h = MSCP_CID_UIDH;
    server->c_id.i_uid_l = MSCP_CID_UIDL;
    server->c_flags      = MSCP_CFLAGS;
    server->c_flagmask   = MSCP_CFLAGMASK;
    server->c_hwversion  = MSCP_HW_VERSION;
    server->c_fwversion  = MSCP_FW_VERSION;

    for ( i = 0; i < server->c_numunits; i++ )
        mscpu_init( server, i );
    return server;
}

int mscps_read_buf ( mscps_t *server, void *target, const void *bufdesc, int offset, int count ) {
    return hostif_read_buf( server->hostif, target, bufdesc, offset, count );
}
int mscps_write_buf( mscps_t *server, const void *target, const void *bufdesc, int offset, int count ) {
    return hostif_write_buf( server->hostif, target, bufdesc, offset, count );
}