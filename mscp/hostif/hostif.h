/**
 * Contains definitions for the MSCP UNIBUS/QBUS host interface
 * implementation.
 * 
 */
#ifndef __hostif__
#define __hostif__

#include <stdint.h>
#include <stddef.h>
#include "mscp/mscp.h"
#include "mscp/hostif/sareg.h"
#include "mscp/hostif/commarea.h"

#define FATAL_ENV_PKT_READ  (1)
#define FATAL_ENV_PKT_WRITE (2)
#define FATAL_CTRL_MEM_PAR  (3)
#define FATAL_CTRL_RAM_PAR  (4)
#define FATAL_CTRL_ROM_PAR  (5)
#define FATAL_RING_READ     (6)
#define FATAL_RING_WRITE    (7)
#define FATAL_INT_MASTER    (8)
#define FATAL_HOST_TIMEOUT  (9)
#define FATAL_CREDIT_EXCEED (10)
#define FATAL_BUSMASTER_ERR (11)
#define FATAL_DIAG_FATAL    (12)
#define FATAL_INT_WRITE     (13)

#define STEP_READY    (-1)
#define STEP_DIAGTA   (-2)
#define STEP_REINIT   (-3)
#define STEP_TRYSTART (-4)
#define STEP_DIAGPP   (-5)
#define STEP_FATAL    (-6)

#define FEAT_22BIT    (1)
#define FEAT_MAP      (2)
#define FEAT_VEC      (4)
#define FEAT_PURGE    (8)
#define FEAT_ENH_DIAG (16)
#define FEAT_ODD_HOST (32)

#define WHEN_KLESI_CMD  (0x100)
#define WHEN_CTRL_READ  (0x200)
#define WHEN_CTRL_WRITE (0x600)
#define WHEN_DATA_READ  (0x000)
#define WHEN_DATA_WRITE (0x400)
#define WHEN_SA_OP      (0x700)
#define WHEN_INTR_REQ   (0x300)

struct mscpa {
    mscps_t      *server;

    /* State fields */
    int        step;
    int        diag_wr;
    int        diag_pp;
    int        fatal_code;

    /* Port identity fields */
    int        klesi_type;
    int        port_type;
    int        mod_id;
    int        fw_ver;
    int        features;
    uint32_t   addrmask;

    /* Host interface config fields */
    uint32_t   ringbase;
    uint16_t   vector;

    /**
     * The number of slots in the command ring
     */
    int        csize;

    /**
     * The number of slots in the response ring
     */
    int        rsize;

    /**
     * Max number of longwords the host is allowing per DMA transfer
     */
    int        burst;
    int        init_ie;
    int        purge_ie;

    /* Communication state */
    uint32_t   cahdr_base;
    uint32_t   rring_base;
    uint32_t   cring_base;
    
    int           cring_idx;
    int           cring_state;
    hostif_desc_t   cring_desc;
    hostif_envhdr_t cring_hdr;
    mscpc_t      *cring_pkt;

    int             rring_idx;
    int             rring_state;
    int             rring_bufsz;
    hostif_desc_t   rring_desc;
    hostif_envhdr_t rring_hdr;
    mscpc_t        *rring_pkt;

    int        r_fir;
    int        c_poll;
    int        c_fir;
};

void hostif_istep1  ( mscpa_t *a );
void hostif_istep2  ( mscpa_t *a );
void hostif_istep3  ( mscpa_t *a );
void hostif_istep4  ( mscpa_t *a );
void hostif_diagpp  ( mscpa_t *a );
void hostif_diagwrap( mscpa_t *a );

int  hostif_cring_poll(  mscpa_t *a );
void hostif_cring_reset( mscpa_t *a );

int hostif_send_ring_irq( mscpa_t *a, int c, int r );

int hostif_rring_do( mscpa_t *a );
void hostif_rring_reset( mscpa_t *a );
void hostif_loop(  mscpa_t *a );
int hostif_ringxfer_err( mscpa_t *a, int status, int fcode );
void hostif_fatal( mscpa_t *a, int fatal_code );
int hostif_read_buf ( mscpa_t *hostif, void *target, const void *bufdesc, int offset, int count );
int hostif_write_buf( mscpa_t *hostif, const void *target, const void *bufdesc, int offset, int count );

#endif