#include "mscp/mscp.h"
#include "lesi/lesi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "projconfig.h"
#include "mscp/packet.h"

void mscp_startup( mscpa_t *a ) {
    int status;
    uint16_t lesi_sr;

    status = lesi_read_reg( LESI_REG_STATUS, &lesi_sr );
    if ( status )
        goto err;
    a->klesi_type = lesi_sr & LESI_SR_IDENT_MASK;

    a->mod_id     = MSCP_MOD_ID;
    a->fw_ver     = MSCP_FW_VERSION;
    a->port_type  = MSCP_PORT_TYPE;
    a->features   = MSCP_BASE_FEATURES;

    /* Handle the different possible KLESI host adapters */
    switch ( a->klesi_type ) {
        case LESI_SR_IDENT_QBUS:
            printf("MSCP Server starting up for KLESI-QA adapter\n");
            a->features |= FEAT_22BIT | FEAT_MAP | FEAT_VEC;
            a->addrmask = MSCP_DESC_22ADDR_MASK; 
            break;
        case LESI_SR_IDENT_UNIBUS:
            printf("MSCP Server starting up for KLESI-UA adapter\n");
            a->features |= FEAT_PURGE | FEAT_MAP | FEAT_VEC;
            a->addrmask = MSCP_DESC_18ADDR_MASK;
            break;
        default:
            printf("Unknown adapter\n");
            status = ERR_MISMATCH;
            goto err;
    }
    
    /* Go to initialization step 1 */
    a->step = 1;

    return;


err:
    if ( status == ERR_INIT ) {
        a->step = STEP_REINIT;
        return;
    }
    a->step = STEP_TRYSTART;
}

void mscp_send_ring_irq( mscpa_t *a ) {
    if ( a->vector ) {
        printf("MSCP: TODO Sending ring transition IRQ\n");

    }

}

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

int mscp_run_command( mscpa_t *a, mscpc_t *cmd ) {
    mscp_pkt_t *pkt = (void *)cmd->data;
    switch( pkt->m_opcode ) {
        case M_OP_STCON:
            printf("MSCP: Received SET CONTROLLER CHARACTERISTICS\n");
            printf("MSCP:    MSCP Version     = %i\n", pkt->m_un.m_setcntchar.Ms_version );
            printf("MSCP:    Controller flags = %04X\n", pkt->m_un.m_setcntchar.Ms_cntflgs );
            printf("MSCP:    Host timeout     = %i s\n", pkt->m_un.m_setcntchar.Ms_hsttmo );
            printf("MSCP:    Wallclock time   = %lli clunks\n", pkt->m_un.m_setcntchar.Ms_time );
            break;
        default:
            printf("Got unknown packet: opcode = %i\n", pkt->m_opcode );
            hexdump(cmd->data, cmd->data_len);
            return 1;
    }
    return 1;
}

int mscp_do_cring( mscpa_t *a ) {
    uint16_t sr;
    mscpc_t *cmd;
    int status;
    if ( a->c_poll ) {
        status = mscp_poll_cring( a );
        if ( status )
            goto err;
    } else {
        status = lesi_read_reg( LESI_REG_STATUS, &sr ) ;
        if ( status )
            goto err;
        if ( status & LESI_SR_POLL ) {
            printf("MSCP C: Got POLL, resuming polling.\n");\
            a->c_poll = 1;
        }
    }
    while ( (cmd = a->cmd_queue) != NULL ) {
        if ( cmd->state != CS_QUEUED )
            break;
        if ( !mscp_run_command( a, cmd ) )
            break;
        a->cmd_queue = cmd->next;
        free( cmd->data );
        free( cmd );
    }
    return ERR_OK;
err:
    if ( status == ERR_INIT ) {
        a->step = STEP_REINIT;
        return status;
    }
    printf("Non reinit error %i: ", status);

}

void mscp_loop(  mscpa_t *a ) {
    uint16_t sr;
    uint16_t sa;
    int status;
    //printf( "MSCP main loop in step %i\n", a->step );
    switch( a->step ) {
        case STEP_TRYSTART:  mscp_startup ( a ); break;
        case 1            :  mscp_istep1  ( a ); break;
        case 2            :  mscp_istep2  ( a ); break;
        case 3            :  mscp_istep3  ( a ); break;
        case 4            :  mscp_istep4  ( a ); break;
        case 5            :  mscp_istep4  ( a ); break;
        case STEP_DIAGTA  :  mscp_diagwrap( a ); break;
        case STEP_DIAGPP  :  mscp_diagpp  ( a ); break;
        case STEP_READY   : 
            mscp_do_cring( a );
            break;
        default           :
            printf("Unknown step in MSCP loop: %i\n", a->step);
        case STEP_REINIT  :
            printf("\n---------------------------------------------------------------------------\n");
            printf("\nMSCP got initialize request.\n");
            lesi_read_reg(LESI_REG_STATUS, &sr);
            lesi_clear_init();
            lesi_read_reg(LESI_REG_STATUS, &sr);
            lesi_write_ram_word(0,0);
            lesi_lowlevel_reset_klesi();
            a->step = STEP_TRYSTART;
            break;
    }
    return;
}