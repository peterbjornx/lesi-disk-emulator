#include "mscp/server/server.h"
#include <ctype.h>
#include "bsp/board.h"
#include "tusb.h"
#include "class/msc/msc.h"
#include "class/msc/msc_host.h"
#include <stdlib.h>

#define USBDRV_BUF_SZ 1024

static mscps_t *usbdrv_server;
static mscpu_t *usbdrv_unit;

#define UMS_IDLE    (0)
#define UMS_REQUSB  (1)
#define UMS_USBDONE (2)
#define UMS_REISSUE (3)

typedef struct usbdrv_ctx {
    /* USB Bus address of backing device */
    uint8_t bus_addr; 

    /* SCSI LUN of backing device */
    uint8_t lun;

    /* Inquiry response */
    scsi_inquiry_resp_t inq;

    int busy;

} usbdrv_ctx_t;

typedef struct usbdrv_cmd {
    mscpu_t  *unit;
    uint8_t   buf [USBDRV_BUF_SZ / 2];
    uint8_t   hbuf[USBDRV_BUF_SZ / 2];
    int       buf_pos;
    uint32_t  cur_lba;
    int       turnsz;
    int       state;
} usbdrv_cmd_t;

bool usbdrv_inq_cb(uint8_t dev_addr, tuh_msc_complete_data_t const * cb_data);

void usbmsc_init( mscps_t *server, int idx ) {
    mscpu_t *unit = server->c_unit + idx;

    board_init();
    tuh_init(0);

    unit->u_drvctx = malloc( sizeof(usbdrv_ctx_t) );  
    memset( unit->u_drvctx, 0, sizeof(usbdrv_ctx_t));
    usbdrv_unit = unit;
    usbdrv_server = server;
    //TODO: Check OOM

}

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

void tuh_mount_cb(uint8_t dev_addr) {
    printf("USBDRV: Got  mount!\n");
  (void) dev_addr;
}

void tuh_umount_cb(uint8_t dev_addr) {
  (void) dev_addr;
}


void usbmsc_process() {
    tuh_task();
}

void tuh_msc_mount_cb(uint8_t dev_addr) {
    usbdrv_ctx_t *ctx = usbdrv_unit->u_drvctx;
    //TODO: Handle multiple devices
    printf("USBDRV: Got mass storage mount!\n");

    ctx->lun = 0;
    ctx->bus_addr = dev_addr;

    tuh_msc_inquiry(ctx->bus_addr, ctx->lun, &ctx->inq, usbdrv_inq_cb, (uintptr_t) usbdrv_unit);
}

void tuh_msc_umount_cb(uint8_t dev_addr) {
    usbdrv_ctx_t *ctx = usbdrv_unit->u_drvctx;
    printf("USBDRV: Got mass storage unmount!\n\n");

    uint8_t const drive_num = dev_addr-1;
    //TODO: Handle unmount
}

static bool usbdrv_io_cmpl(uint8_t dev_addr, tuh_msc_complete_data_t const * cb_data );

int usbdrv_start( mscpu_t *unit, mscpc_t *cmd ) {
    int status;
    usbdrv_ctx_t *ctx  = unit->u_drvctx;
    usbdrv_cmd_t *dcmd = cmd->dctx;
    
    if ( ctx->busy )
        return 0;


    dcmd->turnsz = USBDRV_BUF_SZ;
    //if ( cmd->pkt->m_opcode == M_OP_COMP )
        dcmd->turnsz /= 2; /* half the buffer is used for comparison */
    if ( (cmd->pkt->m_un.m_generic.Ms_bytecnt - dcmd->buf_pos) < dcmd->turnsz )
        dcmd->turnsz = cmd->pkt->m_un.m_generic.Ms_bytecnt - dcmd->buf_pos;

    dcmd->state = UMS_REQUSB;

    switch( cmd->pkt->m_opcode ) {
        case M_OP_ACCES:
            cmd->state = CMD_REPLY;
            cmd->resp->m_status = M_ST_SUCC;
            return 0;
        case M_OP_COMP:
            status = mscps_read_buf( unit->u_server, dcmd->hbuf,
                &cmd->pkt->m_un.m_generic.Ms_buf, dcmd->buf_pos, dcmd->turnsz );
        case M_OP_READ:
            ctx->busy = 1;
            //printf("USBDRV: Issuing read for %p: LBA=%i Count=%i\n", cmd, dcmd->cur_lba, dcmd->turnsz);
            status = tuh_msc_read10( ctx->bus_addr, ctx->lun, dcmd->buf, 
                dcmd->cur_lba, dcmd->turnsz / unit->u_blksize
                , usbdrv_io_cmpl, (uintptr_t) cmd);
            //printf("result: %i\n", status);
            return 0;
        case M_OP_WRITE:
            status = mscps_read_buf( unit->u_server, dcmd->buf,
                &cmd->pkt->m_un.m_generic.Ms_buf, dcmd->buf_pos, dcmd->turnsz );
        case M_OP_ERASE:
            ctx->busy = 1;
            //printf("USBDRV: Issuing write for %p LBA=%i Count=%i\n", cmd, dcmd->cur_lba, dcmd->turnsz);
            status = tuh_msc_write10( ctx->bus_addr, ctx->lun, dcmd->buf, 
                dcmd->cur_lba,
                dcmd->turnsz / unit->u_blksize, usbdrv_io_cmpl, (uintptr_t) cmd);
            //printf("result: %i\n", status);
            return 0;
    }

}

int usbdrv_issue( mscpu_t *unit, mscpc_t *cmd ) {
    int status;
    usbdrv_ctx_t *ctx  = unit->u_drvctx;
    usbdrv_cmd_t *dcmd = cmd->dctx;

    if ( !mscpu_verify_access( unit, cmd ) ) {
        cmd->state = CMD_REPLY;
        return 0;
    }

    dcmd->buf_pos = 0;
    dcmd->cur_lba = cmd->pkt->m_un.m_generic.Ms_lba;

    return usbdrv_start( unit, cmd );
}

int usbdrv_abort( mscpu_t *unit, mscpc_t *cmd ) {
    return 0;//TODO: Actually abort
}


int usbdrv_usbdone( mscpu_t *unit, mscpc_t *cmd ) {
    int status;
    usbdrv_cmd_t *dcmd = cmd->dctx;
    usbdrv_ctx_t *ctx  = unit->u_drvctx;

    //printf("USBDRV: Got USB done for %p\n", cmd);
    if ( cmd->state == CMD_ABORTING ) {
        cmd->state = CMD_REPLY;
        return true;
    }

    if ( cmd->state != CMD_ACTIVE ) {
        //TODO: what to do if we get here?
        return true;
    }

    dcmd->state = UMS_IDLE;

    /* Handle data from disk */
    if ( cmd->pkt->m_opcode == M_OP_READ ) {
        status = mscps_write_buf( unit->u_server, dcmd->buf,
            &cmd->pkt->m_un.m_generic.Ms_buf, dcmd->buf_pos, dcmd->turnsz );
        if ( status )
            printf("error dumping read io into mem: %i\n", status);
    } else if ( cmd->pkt->m_opcode == M_OP_COMP ) {
        if ( memcmp( dcmd->buf, dcmd->hbuf, dcmd->turnsz ) != 0 ) {
            cmd->resp->m_status = M_ST_COMP;
            cmd->state = CMD_REPLY;
            return true;
        }
    }

    /* Move to next sector */
    dcmd->cur_lba += dcmd->turnsz / unit->u_blksize;
    dcmd->buf_pos += dcmd->turnsz;

    if ( dcmd->buf_pos == cmd->pkt->m_un.m_generic.Ms_bytecnt ) {
        /* We're done */
        cmd->resp->m_status = M_ST_SUCC;
        cmd->state = CMD_REPLY;
        return true;
    }

    status = usbdrv_start( unit, cmd );
    if ( status ) {
        printf( "USBDRV: Transfer restart error: %i\n", status );
    }
    return true;
}

int usbdrv_proc( mscpu_t *unit, mscpc_t *cmd ) {
    int status;
    usbdrv_ctx_t *ctx  = unit->u_drvctx;
    usbdrv_cmd_t *dcmd = cmd->dctx;

    /* Ignore commands that are owned by the unit driver */
    if ( cmd->state == CMD_COMPLETE || cmd->state == CMD_DELETE || cmd->state == CMD_REPLY )
        return 0;

    if ( cmd->state == CMD_QUEUED ) {
        cmd->dctx = malloc( sizeof(usbdrv_cmd_t) );

        if ( cmd->dctx == NULL )
            return 0; /* try again on next spin */

        memset( cmd->dctx, 0, sizeof(usbdrv_cmd_t) );

        dcmd = cmd->dctx;
        cmd->state = CMD_ACTIVE;
        dcmd->state = UMS_REISSUE;
        dcmd->unit = unit;
        status = usbdrv_issue( unit, cmd );
    } else if ( cmd->state == CMD_ABORTED ) {
        cmd->state = CMD_ABORTING;
        status = usbdrv_abort( unit, cmd );
    } else if ( dcmd->state == UMS_USBDONE ) {
        status = usbdrv_usbdone( unit, cmd );
    } else if ( dcmd->state == UMS_REISSUE ) {
        status = usbdrv_issue( unit, cmd );
    }
    if ( cmd->state == CMD_REPLY || cmd->state == CMD_DELETE ) {
        if ( cmd->dctx ) {
            free( cmd->dctx );
            cmd->dctx = NULL;
        }
    }
    return status;
}

static bool usbdrv_io_cmpl(uint8_t dev_addr, tuh_msc_complete_data_t const * cb_data ) {
    int bufpos;
    int status;
    mscpc_t *cmd       = (void *) cb_data->user_arg;
    usbdrv_cmd_t *dcmd = cmd->dctx;
    mscpu_t *unit      = dcmd->unit;
    usbdrv_ctx_t *ctx  = unit->u_drvctx;
    
    ctx->busy = 0;

    //printf("USBDRV: Got IO completion for %p\n", cmd);

    if ( cmd->state == CMD_ABORTING ) {
        cmd->state = CMD_REPLY;
        return true;
    }

    if ( cmd->state != CMD_ACTIVE ) {
        //TODO: what to do if we get here?
        return true;
    }

    dcmd->state = UMS_USBDONE;
    return true;
}

bool usbdrv_inq_cb(uint8_t dev_addr, tuh_msc_complete_data_t const * cb_data) {
    mscpu_t *unit        = (void *) cb_data->user_arg;
    usbdrv_ctx_t *ctx    = unit->u_drvctx;
    msc_cbw_t const* cbw = cb_data->cbw;
    msc_csw_t const* csw = cb_data->csw;

    if (csw->status != 0) {
        printf("USBDEV: Inquiry failed\n");
        return false;
    }

    printf("USBDRV: %.8s %.16s rev %.4s\n", 
        ctx->inq.vendor_id, ctx->inq.product_id, ctx->inq.product_rev);

    memcpy( &unit->u_id.i_uid_h, ctx->inq.product_id, 6 );
    unit->u_id.i_class = M_CC_DISK144;
    unit->u_id.i_model = M_CM_UDA50;
    unit->u_spindles = 1;
    unit->u_mediaid  = 0x254B3294;
    unit->u_blkcount = tuh_msc_get_block_count(dev_addr, cbw->lun);
    unit->u_blksize  = tuh_msc_get_block_size(dev_addr, cbw->lun);

    mscpu_set_avail( unit->u_server, unit->u_idx, usbdrv_proc );

    return true;
}
