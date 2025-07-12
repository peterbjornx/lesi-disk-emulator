#include "mscp/server/server.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "error.h"

void mscpu_init( mscps_t *server, int idx ) {
    mscpu_t *unit;

    /* Get unit pointer */
    unit = server->c_unit + idx;
    
    /* Initialize fields linking unit to server */
    memset( unit, 0, sizeof(mscpu_t) );
    unit->u_idx    = idx;
    unit->u_server = server;

    /* Initialize command queue */
    unit->cq_head = unit->cq_tail = NULL;
    unit->cq_count = 0;
    
    /* Set unit offline as there isn't yet a back end */
    unit->u_state = MUS_OFFLINE;

}

void mscpu_set_avail( mscps_t *server, int idx, mscpu_proc_cmd_t drvproc ) {
    mscpu_t *unit;

    /* Get unit pointer */
    unit = server->c_unit + idx;

    if ( drvproc )
        unit->u_proccb = drvproc;
    
    if ( unit->u_state == MUS_OFFLINE )
        unit->u_state = MUS_AVAIL;
    
    //TODO: send attention message

}

static void mscpu_offline_err( mscpu_t *unit, mscp_resp_t *end ) {
    printf("MSCP:    Failed: Unit %i is \"Unit-Offline\"\n", unit->u_idx );
    end->m_status = M_ST_OFFLN; // TODO: Sub status
}

static void mscpu_avail_err( mscpu_t *unit, mscp_resp_t *end ) {
    printf("MSCP:    Failed: Unit %i is \"Unit-Available\"\n", unit->u_idx );
    end->m_status = M_ST_AVLBL; // TODO: Sub status
}

int mscpu_enqueue( mscpu_t *unit, mscpc_t *cmd ) {
    if ( unit->cq_tail ) {
        unit->cq_tail->next = cmd;
    } else {
        unit->cq_head = cmd;
    }
    cmd->next  = NULL;
    cmd->state = CMD_QUEUED;
    unit->cq_tail = cmd;
    unit->cq_count++;
}

int mscpu_reinit( mscpu_t *unit ) {
    //TODO: Implement
    return ERR_OK;
}

int mscpu_process( mscpu_t *unit ) {
    mscpc_t *cmd, **prev;
    int status;
    for ( cmd = unit->cq_head, prev = &unit->cq_head; cmd != NULL; cmd = cmd->next ) {
        if ( unit->u_proccb ) {
            status = unit->u_proccb( unit, cmd );
            if ( status )
                return status;
        }
        if ( cmd->state == CMD_DELETE || cmd->state == CMD_REPLY ) {
            /* These states are requests to remove the command from unit
               processing. Proceed to unlink the command in place.
             */
            *prev = cmd->next;
            if ( cmd == unit->cq_tail )
                unit->cq_tail = NULL;
            unit->cq_count--;
        }
        if ( cmd->state == CMD_REPLY ) {
            mscps_send_end( unit->u_server, cmd );
        } else if ( cmd->state == CMD_DELETE )
            mscps_cmd_free( cmd );
    }
    return ERR_OK;
}

static int _mscpu_setchar( mscpu_t *unit, mscp_pkt_t *pkt,  mscp_resp_t *end, int *sz, int onl ) {
    printf("MSCP:    Unit flags       = %04X\n"       , pkt->m_un.m_online.Ms_unitflgs );
    printf("MSCP:    Dev. dep params  = %08X\n"       , pkt->m_un.m_online.Ms_ddp );
    if ( unit->u_state == MUS_OFFLINE ) {
        mscpu_offline_err( unit, end );
        goto error;        
    } else if ( unit->u_state == MUS_AVAIL ) {
        if ( onl ) {
            printf("MSCP: Unit %i transitioned to \"Unit-Online\"\n", unit->u_idx );
            unit->u_state = MUS_ONLINE;
        } else {
            mscpu_avail_err( unit, end );
            goto error;
        }
    }
    end->m_status = M_ST_SUCC;
    unit->u_flags &= ~unit->u_flagmask;
    unit->u_flags |= pkt->m_un.m_online.Ms_unitflgs & unit->u_flagmask;
    end->m_un.m_online.Ms_unitflgs  = unit->u_flags;
    end->m_un.m_online.Ms_media     = unit->u_mediaid;
    end->m_un.m_online.Ms_multiunit = 0;
    end->m_un.m_online.Ms_spindles  = unit->u_spindles;
    end->m_un.m_online.Ms_unitid    = unit->u_id;
    end->m_un.m_online.Ms_vsn       = unit->u_vid;
    end->m_un.m_online.Ms_size      = unit->u_blkcount;
error:
    *sz = 44;
    return ERR_OK;
}

void mscpu_abort_cmd( mscpu_t *unit, mscpc_t *cmd ) {
    //TODO: Actually cancel any outstanding transactions on the command
    cmd->resp->m_status = M_ST_ABRTD; //TODO: Sub code
    cmd->state          = CMD_ABORTED;
}

int mscpu_online( mscpu_t *unit, mscp_pkt_t *pkt,  mscp_resp_t *end, int *sz ) {
    printf("MSCP: Received ONLINE\n");
    return _mscpu_setchar( unit, pkt, end, sz, 1 /* set ONLINE */ );
}

int mscpu_setchar( mscpu_t *unit, mscp_pkt_t *pkt,  mscp_resp_t *end, int *sz ) {
    printf("MSCP: Received SET UNIT CHARACTERISTICS\n");
    return _mscpu_setchar( unit, pkt, end, sz, 0 /* not ONLINE */ );
}

int mscpu_abort( mscpu_t *unit, mscp_pkt_t *pkt,  mscp_resp_t *end, int *sz ) {
    mscpc_t *cmd;
    printf("MSCP: Received ABORT\n");
    printf("MSCP:    ORN              = %i\n"       , pkt->m_un.m_abort.Ms_orn );
    for ( cmd = unit->cq_head; cmd != NULL; cmd = cmd->next ) {
        if ( cmd->pkt->m_cmdref != pkt->m_un.m_abort.Ms_orn )
            continue;
        mscpu_abort_cmd( unit, cmd );
    }
    end->m_status = M_ST_SUCC;
    end->m_un.m_abort.Ms_orn = pkt->m_un.m_abort.Ms_orn;
    *sz = 16;
    return ERR_OK;
}

int mscpu_verify_access( mscpu_t *unit, mscpc_t *cmd ) {
    if ( unit->u_state == MUS_AVAIL ) {
        mscpu_avail_err( unit, cmd->resp );
        return 0;
    } else if ( unit->u_state == MUS_OFFLINE ) {
        mscpu_offline_err( unit, cmd->resp );
        return 0;
    } else if ( cmd->pkt->m_opcode == M_OP_WRITE || cmd->pkt->m_opcode == M_OP_ERASE ) {
        if ( unit->u_flags & M_UF_WRTPH ) {
            cmd->resp->m_status  = M_ST_WRTPR;
            cmd->resp->m_status |= M_SC_HARDW << M_ST_SBBIT;
            return 0;
        }
        if ( unit->u_flags & M_UF_WRTPS ) {
            cmd->resp->m_status  = M_ST_WRTPR;
            cmd->resp->m_status |= M_SC_SOFTW << M_ST_SBBIT;
            return 0;
        }
    }
    if ( cmd->pkt->m_un.m_generic.Ms_lba > unit->u_blkcount ) {
        cmd->resp->m_status = M_ST_ICMD;
        cmd->resp->m_status |= 28 << M_ST_SBBIT;
        return 0;
    }
    if ( cmd->pkt->m_un.m_generic.Ms_bytecnt == 0 ) {
        cmd->resp->m_status = M_ST_SUCC;
        return 0;
    }
    return 1;
}

int mscpu_access( mscpu_t *unit, mscp_pkt_t *pkt,  mscp_resp_t *end, int *sz ) {
    printf("MSCP: Received ACCESS\n");
    printf("MSCP:    Byte count       = %i\n"       , pkt->m_un.m_generic.Ms_bytecnt );
    printf("MSCP:    LBA              = %i\n"       , pkt->m_un.m_generic.Ms_lba );
    end->m_status = M_ST_SUCC;
    end->m_un.m_generic.Ms_bytecnt = end->m_un.m_generic.Ms_bytecnt;
    end->m_un.m_generic.Ms_lba = 0;
    *sz = 32;
    return ERR_OK;
}