#include <stdint.h>
#include <stddef.h>
#include "mscp/sareg.h"
#include "mscp/idlist.h"
#include "mscp/commarea.h"

#define STEP_READY    (-1)
#define STEP_DIAGTA   (-2)
#define STEP_REINIT   (-3)
#define STEP_TRYSTART (-4)
#define STEP_DIAGPP   (-5)

#define FEAT_22BIT    (1)
#define FEAT_MAP      (2)
#define FEAT_VEC      (4)
#define FEAT_PURGE    (8)
#define FEAT_ENH_DIAG (16)
#define FEAT_ODD_HOST (32)

typedef struct mscpc mscpc_t;

typedef struct {
    /* State fields */
    int      step;
    int      diag_wr;
    int      diag_pp;

    /* Port identity fields */
    int      klesi_type;
    int      port_type;
    int      mod_id;
    int      fw_ver;
    int      features;
    uint32_t addrmask;

    /* Host interface config fields */
    uint32_t ringbase;
    uint16_t vector;

    /**
     * The number of slots in the command ring
     */
    int      csize;

    /**
     * The number of slots in the response ring
     */
    int      rsize;

    /**
     * Max number of longwords the host is allowing per DMA transfer
     */
    int      burst;
    int      init_ie;
    int      purge_ie;

    /* Communication state */
    uint32_t cahdr_base;
    uint32_t rring_base;
    uint32_t cring_base;
    
    int      cring_idx;
    int      rring_idx;

    mscpc_t  *cmd_queue;

    int      c_poll;
    int      c_fir;
} mscpa_t;

#define CS_UNUSED    (0)
#define CS_XFER_ENV  (1)
#define CS_XFER_PAYL (2)
#define CS_XFER_OWN  (3)
#define CS_QUEUED    (4)
#define CS_SENDIRQ   (5)

struct mscpc {
    mscpc_t      *next;
    int           state;
    mscp_desc_t   desc;
    mscp_envhdr_t hdr;
    size_t        data_len;
    int           msg_type;
    int           credit;
    void         *data;
};

void mscp_istep1  ( mscpa_t *a );
void mscp_istep2  ( mscpa_t *a );
void mscp_istep3  ( mscpa_t *a );
void mscp_istep4  ( mscpa_t *a );
void mscp_diagpp  ( mscpa_t *a );
void mscp_diagwrap( mscpa_t *a );

int  mscp_poll_cring(  mscpa_t *a );

void mscp_startup (  mscpa_t *a );
void mscp_loop    (  mscpa_t *a );
