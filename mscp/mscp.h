#ifndef __mscp__ 
#define __mscp__

#include <stdint.h>
#include <stddef.h>
#include "mscp/idlist.h"

#define CS_UNUSED    (0)
#define CS_XFER_ENV  (1)
#define CS_XFER_PAYL (2)
#define CS_XFER_OWN  (3)
#define CS_QUEUED    (4)
#define CS_SENDIRQ   (5)
#define CS_WAITFULL  (6)
#define CS_XFERSZ    (7)

#define CMD_QUEUED   (0)
#define CMD_ACTIVE   (1)
#define CMD_COMPLETE (2)
#define CMD_ABORTED  (3)
#define CMD_DELETE   (4)
#define CMD_REPLY    (5)
#define CMD_ABORTING (6)

typedef struct mscpa mscpa_t;
typedef struct mscps mscps_t;
typedef struct mscpc mscpc_t;
typedef struct mscpu mscpu_t;

#include "mscp/packet.h"

/**
 * MSCP packet as received/sent down the line
 */
struct mscpc {
    mscpc_t           *next;
    size_t             data_len;
    int                conn_id;
    int                msg_len;
    int                msg_type;
    int                credit;
    int                state;
    union {
        void          *data;
        mscp_pkt_t    *pkt;
        mscp_resp_t   *resp;
        mscp_errlog_t *errl;
    };
    void              *dctx;
};

void hostif_set_server( mscpa_t *hostif, mscps_t *server );
mscpa_t *hostif_setup(  );
int  hostif_send_response(  mscpa_t *a, mscpc_t *resp );
void mscps_reinit( mscps_t *server );
void mscps_enqueue_cmd( mscps_t *server, mscpc_t *cmd );
void mscps_attach( mscps_t *server ,mscpa_t *hostif );
mscps_t *mscps_setup( );
void mscps_loop( mscps_t *server ) ;

void hostif_loop(  mscpa_t *a );

#endif