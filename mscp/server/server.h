#ifndef __mserver__
#define __mserver__

#include "mscp/mscp.h"

#define MUS_OFFLINE (0) /* Unit-Offline   */
#define MUS_AVAIL   (1) /* Unit-Available */
#define MUS_ONLINE  (2) /* Unit-Online    */

typedef int (*mscpu_proc_cmd_t)( mscpu_t *unit, mscpc_t *cmd );

struct mscps {
    mscpa_t *hostif;

    /* Command queue fields */
    mscpc_t *cq_tail;
    mscpc_t *cq_head;
    int      cq_count;

    /* Response queue fields */
    mscpc_t *rq_tail;
    mscpc_t *rq_head;
    int      rq_count;

    /* Controller fields */

    /** Controller ID */
    mscp_id_t c_id;
    /** Hardware version */
    uint8_t   c_hwversion;
    /** Firmware version */
    uint8_t   c_fwversion;

    /** Controller flags  */
    uint16_t  c_flags;

    uint16_t  c_flagmask;

    int       c_numunits;
    mscpu_t  *c_unit;
};

struct mscpu {
    mscps_t * u_server;
    int       u_idx;
    int       u_state;

    /* Command queue */
    mscpc_t  *cq_head;
    mscpc_t  *cq_tail;
    int       cq_count;

    /* Identity and characteristics of unit */
    mscp_id_t u_id;
    uint32_t  u_vid;
    int       u_spindles;
    uint32_t  u_mediaid;
    uint32_t  u_blkcount;
    uint16_t  u_blksize;
    mscpu_proc_cmd_t u_proccb;

    /** Unit flags  */
    uint16_t  u_flags;
    uint16_t  u_flagmask;

    void     *u_drvctx;

};


int mscps_send_rq( mscps_t *server );
void mscps_send_response( mscps_t *server, void *end, int conn, int sz, int type );
void mscps_send_end     ( mscps_t *server, mscpc_t *pkt );

/* Controller packets */
int mscp_cntrl_scc( mscps_t *srv, mscp_pkt_t *pkt,  mscp_resp_t *end, int *sz );

int mscpu_enqueue( mscpu_t *unit, mscpc_t *cmd );
int mscpu_online ( mscpu_t *unit, mscp_pkt_t *pkt,  mscp_resp_t *end, int *sz );
int mscpu_setchar( mscpu_t *unit, mscp_pkt_t *pkt,  mscp_resp_t *end, int *sz );
int mscpu_access ( mscpu_t *unit, mscp_pkt_t *pkt,  mscp_resp_t *end, int *sz );
int mscpu_abort  ( mscpu_t *unit, mscp_pkt_t *pkt,  mscp_resp_t *end, int *sz );
void mscpu_abort_cmd( mscpu_t *unit, mscpc_t *cmd );
void mscps_cmd_free  ( mscpc_t *cmd );

void mscpu_init( mscps_t *server, int idx );
int mscpu_process( mscpu_t *unit );
int mscpu_reinit( mscpu_t *unit );
int mscps_read_buf ( mscps_t *server, void *target, const void *bufdesc, int offset, int count );
int mscps_write_buf( mscps_t *server, const void *target, const void *bufdesc, int offset, int count );

#endif