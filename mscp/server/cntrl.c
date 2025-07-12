#include "mscp/server/server.h"
#include <stdlib.h>
#include <stdio.h>
#include "error.h"

int mscp_cntrl_scc( mscps_t *srv, mscp_pkt_t *pkt,  mscp_resp_t *end, int *sz ) {
    printf("MSCP: Received SET CONTROLLER CHARACTERISTICS\n");
    printf("MSCP:    MSCP Version     = %i\n"         , pkt->m_un.m_setcntchar.Ms_version );
    printf("MSCP:    Controller flags = %04X\n"       , pkt->m_un.m_setcntchar.Ms_cntflgs );
    printf("MSCP:    Host timeout     = %i s\n"       , pkt->m_un.m_setcntchar.Ms_hsttmo );
    printf("MSCP:    Wallclock time   = %lli clunks\n", pkt->m_un.m_setcntchar.Ms_time );
    srv->c_flags = pkt->m_un.m_setcntchar.Ms_cntflgs & srv->c_flagmask;

    end->m_status = M_ST_SUCC;
    end->m_un.m_setcntchar.Ms_id      = srv->c_id;
    end->m_un.m_setcntchar.Ms_chvrsn  = srv->c_hwversion;
    end->m_un.m_setcntchar.Ms_csvrsn  = srv->c_fwversion;
    end->m_un.m_setcntchar.Ms_timeout = 30;
    end->m_un.m_setcntchar.Ms_cntflgs = srv->c_flags;

    *sz = 32;
    return ERR_OK;
}