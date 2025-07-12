#include "mscp/hostif/hostif.h"
#include "lesi/lesi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "projconfig.h"
#include "mscp/packet.h"

void hostif_startup( mscpa_t *a ) {
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
            printf("MSCP Host interface starting up for KLESI-QA adapter\n");
            a->features |= FEAT_22BIT | FEAT_MAP | FEAT_VEC;
            a->addrmask = MSCP_DESC_22ADDR_MASK; 
            break;
        case LESI_SR_IDENT_UNIBUS:
            printf("MSCP Host interface starting up for KLESI-UA adapter\n");
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

int hostif_send_ring_irq( mscpa_t *a, int c, int r ) {
    uint16_t v= 1;
    int status;
    //printf("HostIF: Sending ring transition IRQ\n");
    if ( c ) {
        status = lesi_set_host_addr( a->cahdr_base + 4 );
        propagateTagged( status, WHEN_INTR_REQ );

        status = lesi_write_dma( &v, 1 );
        propagateTagged( status, WHEN_INTR_REQ );
    }
    if ( r ) {
        status = lesi_set_host_addr( a->cahdr_base + 6 );
        propagateTagged( status, WHEN_INTR_REQ );

        status = lesi_write_dma( &v, 1 );
        propagateTagged( status, WHEN_INTR_REQ );
    }
    if ( a->vector ) {
        lesi_send_intr( a->vector );
        lesi_lowlevel_wait_ready();
    }
    return ERR_OK;

}

int hostif_process( mscpa_t *a ) {
    int status;

    status = hostif_cring_poll( a );
    propagate( status );

    status = hostif_rring_do( a );
    propagate( status );

    return ERR_OK;
}

void hostif_reinit( mscpa_t *a ) {
    a->step = STEP_REINIT;
}

void hostif_active_loop( mscpa_t *a ) {
    int status, err, when;

    status = hostif_process( a );

    err  = ERR_STATUS( status );
    when = ERR_WHEN( status );

    if ( err == ERR_INIT ) {
        hostif_reinit( a );
        return;
    } else if ( status & ERR_FATAL ) {
        hostif_fatal( a, a->fatal_code );
        return;
    }
    
    
}

void hostif_fatal( mscpa_t *a, int fatal_code ) {
    lesi_sa_write( SA_ERROR | a->fatal_code );
    //TODO: Interrupt?
    a->step = ERR_FATAL;
    printf("HostIF Fatal error: %i\n", fatal_code);
    return; 
    
}


void hostif_loop(  mscpa_t *a ) {
    uint16_t sr;
    uint16_t sa;
    int status;
    switch( a->step ) {
        case STEP_TRYSTART:  hostif_startup ( a ); break;
        case 1            :  hostif_istep1  ( a ); break;
        case 2            :  hostif_istep2  ( a ); break;
        case 3            :  hostif_istep3  ( a ); break;
        case 4            :  hostif_istep4  ( a ); break;
        case 5            :  hostif_istep4  ( a ); break;
        case STEP_DIAGTA  :  hostif_diagwrap( a ); break;
        case STEP_DIAGPP  :  hostif_diagpp  ( a ); break;
        case STEP_READY   : 
            hostif_active_loop( a );
            break;
        default           :
            printf("HostIF: Unknown step in loop: %i\n", a->step);
        case STEP_REINIT  :
            printf("\n---------------------------------------------------------------------------\n");
            printf("\nHostIF: got initialize request.\n");
            mscps_reinit( a->server );
            lesi_read_reg(LESI_REG_STATUS, &sr);
            lesi_clear_init();
            lesi_read_reg(LESI_REG_STATUS, &sr);
            lesi_write_ram_word(0,0);
            lesi_lowlevel_reset_klesi();
            hostif_cring_reset( a );
            hostif_rring_reset( a );
            a->step = STEP_TRYSTART;
            break;
        case STEP_FATAL:
            if ( lesi_check_init() )
                a->step = STEP_REINIT;
            break;
    }
    return;
}

mscpa_t *hostif_setup(  ) {
    mscpa_t *a;
    a = malloc( sizeof(mscpa_t) );
    if ( a == NULL )
        return NULL;
    memset( a, 0, sizeof(mscpa_t) );
    a->server = NULL;
    a->step = STEP_TRYSTART;
    hostif_cring_reset( a );
    hostif_rring_reset( a );
}

void hostif_set_server( mscpa_t *hostif, mscps_t *server ) {
    hostif->server = server;
}

int hostif_read_buf ( mscpa_t *hostif, void *target, const void *bufdesc, int offset, int count ) {
    int status;
    const uint32_t *bufd = bufdesc;
    //TODO: Verify that no byte transfers are attempted
    //TODO: Support UBA channels & purge
    status = lesi_set_host_addr( (*bufd + offset) & 0xFFFFFF );
    propagate(status);
    
    return lesi_read_dma( target, count / 2 );
}

int hostif_write_buf( mscpa_t *hostif, const void *target, const void *bufdesc, int offset, int count ) {
    int status;
    const uint32_t *bufd = bufdesc;
    //TODO: Verify that no byte transfers are attempted
    //TODO: Support UBA channels & purge
    status = lesi_set_host_addr( (*bufd + offset) & 0xFFFFFF );
    propagate(status);
    
    return lesi_write_dma( target, count / 2 );

}