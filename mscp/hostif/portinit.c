#include "mscp/hostif/hostif.h"
#include "lesi/lesi.h"
#include <stdio.h>
#include "projconfig.h"

void hostif_istep1( mscpa_t *a ) {
    uint16_t sa_out, sa_in;
    int status;

    /* Setup step 1 read register*/
    sa_out = SA_INIT1_STEP;
    if (  a->features & FEAT_22BIT    ) sa_out |= SA_INIT1R_QB;
    if (  a->features & FEAT_MAP      ) sa_out |= SA_INIT1R_MP;
    if ( ~a->features & FEAT_VEC      ) sa_out |= SA_INIT1R_NV;
    if (  a->features & FEAT_ENH_DIAG ) sa_out |= SA_INIT1R_DI;
    if (  a->features & FEAT_ODD_HOST ) sa_out |= SA_INIT1R_OD;
    
    /* Send it to the host */
    status = lesi_sa_write( sa_out );
    if ( status )
        goto err;

    /* Read response */
    status = lesi_sa_read_response( &sa_in );
    if ( status )
        goto err;

    if ( ~sa_in & 0x8000 ) {
        printf("HostIF Step 1 did not have high bit set. Restarting\n");
        return;
    }

    a->vector  = ((sa_in & SA_INIT1W_VADR_MASK ) >> SA_INIT1W_VADR_BIT) * 4;
    a->csize   = (sa_in & SA_INIT1W_CRING_MASK) >> SA_INIT1W_CRING_BIT;
    a->rsize   = (sa_in & SA_INIT1W_RRING_MASK) >> SA_INIT1W_RRING_BIT;
    a->init_ie =  sa_in & SA_INIT1W_IE;
    a->diag_wr =  sa_in & SA_INIT1W_WR;

    printf("HostIF Init sequence step 1: Vector = %04o, C-Ring size: %i, R-Ring size: %i, IE=%i, WR=%i\n",
            a->vector, a->csize, a->rsize, a->init_ie, a->diag_wr );

    if ( a->diag_wr ) {
        printf("HostIF Going to debug wraparound mode until reset\n");
        status = lesi_sa_write( sa_in );
        if ( status )
            goto err;

        a->step = STEP_DIAGTA;
        return;
    }
    
    a->step = 2;

    return;

err:
    if ( status == ERR_INIT ) {
        a->step = STEP_REINIT;
        return;
    }
    lesi_sa_write( sa_out | SA_ERROR );
    printf("Error in step 1: %i, retrying\n", status);
    return;
}

void hostif_istep2( mscpa_t *a ) {
    uint16_t sa_out, sa_in;
    int status;

    /* Setup step 2 read register*/
    sa_out = SA_INIT2_STEP;
    sa_out |= (a->port_type << SA_INIT2R_PTYP_BIT ) & SA_INIT2R_PTYP_MASK;
    sa_out |= (a->csize     << SA_INIT2R_CRING_BIT) & SA_INIT2R_CRING_MASK;
    sa_out |= (a->rsize     << SA_INIT2R_RRING_BIT) & SA_INIT2R_RRING_MASK;
    sa_out |= SA_INIT2R_ALWAYS;
    if ( a->diag_wr ) sa_out |= SA_INIT2R_WR;

    /* Send it to the host */
    if ( a->init_ie && a->vector != 0 )
        status = lesi_sa_intr( a->vector, sa_out );
    else
        status = lesi_sa_write( sa_out );
    if ( status )
        goto err;
        
    /* Read response */
    /* Read response */
    if ( a->init_ie && a->vector != 0 )
        status = lesi_sa_read_response_intr( &sa_in );
    else
        status = lesi_sa_read_response( &sa_in );
    if ( status )
        goto err;

    a->ringbase = (sa_in & SA_INIT2W_RINGBASE_MASK ) >> SA_INIT2W_RINGBASE_BIT;
    a->purge_ie =  sa_in & SA_INIT2W_PI;

    printf("HostIF Init sequence step 2: Ringbase = %07o, PI=%i\n",
            a->ringbase, a->purge_ie );
    
    a->step = 3;

    return;

err:
    if ( status == ERR_INIT ) {
        a->step = STEP_REINIT;
        return;
    }
    lesi_sa_write( sa_out | SA_ERROR );
    printf("Error in step 2: %i, retrying\n", status);
    return;
}

void hostif_istep3( mscpa_t *a ) {
    uint16_t sa_out, sa_in;
    int status;

    /* Clear POLL and PURGED flags */
    status = lesi_read_reg( LESI_REG_CLEAR_POLL,   &sa_in );
    if ( status )
        goto err;

    /* Setup step 3 read register*/
    sa_out = SA_INIT3_STEP;
    sa_out |= ((a->vector/4) << SA_INIT3R_VADR_BIT ) & SA_INIT3R_VADR_MASK;
    if ( a->init_ie ) sa_out |= SA_INIT3R_IE;

    /* Send it to the host */
    if ( a->init_ie && a->vector != 0 )
        status = lesi_sa_intr( a->vector, sa_out );
    else
        status = lesi_sa_write( sa_out );
    if ( status )
        goto err;
        

    /* Read response */
    if ( a->init_ie && a->vector != 0 )
        status = lesi_sa_read_response_intr( &sa_in );
    else
        status = lesi_sa_read_response( &sa_in );
    if ( status )
        goto err;

    status = lesi_read_reg( LESI_REG_CLEAR_PURGED, &sa_out );
    if ( status )
        goto err;

    a->ringbase |= ((sa_in & SA_INIT3W_HRBASE_MASK ) >> SA_INIT3W_HRBASE_BIT) << 16;
    a->diag_pp   =  sa_in & SA_INIT3W_PP;

    printf("HostIF Init sequence step 3: Ringbase = %09o, PP=%i\n",
            a->ringbase, a->diag_pp );

    if ( a->diag_pp ) {
        /*
            This test will perform the first three steps of the init sequence.
            When the host responds to the step 3 transition it will write a one
            bit to bit 15 of the SA register, thereby requesting the execution of
            purge and poll testing. The host then waits for the SA register to 
            transition to a zero value. The host then writes zeros to the SA
            register simulating a "purge completed" host action. the host then
            reads the IP register to simulate a start polling command from the
            host to the port. The test is complete when the controller announces
            the transition to step 4 in the SA register.
        
        */
        a->step = STEP_DIAGPP;
        return;
    }
    
    a->step = 4;

    return;

err:
    if ( status == ERR_INIT ) {
        a->step = STEP_REINIT;
        return;
    }
    lesi_sa_write( sa_out | SA_ERROR );
    printf("Error in step 3: %i, retrying\n", status);
    return;
}
uint16_t buf[32];

void hostif_istep4( mscpa_t *a ) {
    uint16_t sa_out, sa_in, go, lf;
    int status, rsize, csize;

    /* Clear buffers */
    if ( a->step == 4 ) {
        status = lesi_sa_end();
        csize = 1 << a->csize;
        rsize = 1 << a->rsize;
        a->cahdr_base = a->ringbase - sizeof(hostif_cahdr_t);
        a->rring_base = a->ringbase;
        a->cring_base = a->ringbase + 4 * rsize;
        status = lesi_set_host_addr( a->cahdr_base );
        if ( status ) {
            printf("DMA failed!\n");
            goto err;
        }
        status = lesi_write_dma_zeros( csize * 2 + rsize * 2 + sizeof(hostif_cahdr_t) / 2 );
        if ( status ) {
            printf("DMA failed!\n");
            goto err;//TODO: not technically part of the init
        }
        sleep_ms(10);
    
    }

    /* Setup step 4 read register*/
    sa_out = SA_INIT4_STEP;
    sa_out |= ((a->mod_id) << SA_INIT4R_MOD_BIT ) & SA_INIT4R_MOD_MASK;
    sa_out |= ((a->fw_ver) << SA_INIT4R_VER_BIT ) & SA_INIT4R_VER_MASK;

    /* Send it to the host */
    if ( a->init_ie && a->vector != 0 && a->step == 4 )
        status = lesi_sa_intr( a->vector, sa_out );
    else
        status = lesi_sa_write( sa_out );
    if ( status )
        goto err;

    /* Read response */
    if ( a->init_ie && a->vector != 0 && a->step == 4 )
        status = lesi_sa_read_response_intr( &sa_in );
    else
        status = lesi_sa_read_response( &sa_in );
    if ( status )
        goto err;
    go        =  sa_in & SA_INIT4W_GO;

    if ( a->step == 4 ) {
        a->burst |= ((sa_in & SA_INIT4W_BURST_MASK ) >> SA_INIT4W_BURST_BIT);
        lf        =  sa_in & SA_INIT4W_LF;
        //printf( "HostIF Init sequence step 4: Burst = %i, GO=%i LF=%i\n",
         //       a->burst, go, lf );
        if ( a->burst == 0 ) {
            a->burst = MSCP_DEF_BURSTSZ;
            //printf("HostIF Init sequence step 4: Default burst size of %i requested\n", a->burst);
        }
    } else {
        printf( "HostIF Init wait: GO=%i\n", go );
    }

    if ( go ) {
        /* These fields are kept in their original log2 form during init 
           to facilitate their echo back to the host */
        a->rring_idx = 0;
        a->cring_idx = 0;
        a->c_poll = 0;
        a->csize = 1 << a->csize;
        a->rsize = 1 << a->rsize;
        a->step = STEP_READY;
        printf("###############################\n");
        printf("#    MSCP Port initialized    #\n");
        printf("###############################\n\n");
        a->c_poll = 1;
    } else
        a->step = 5;

    return;

err:
    if ( status == ERR_INIT ) {
        a->step = STEP_REINIT;
        return;
    }
    lesi_sa_write( sa_out | SA_ERROR );
    printf("Error in step 4: %i, retrying\n", status);
    return;
}

void hostif_diagwrap( mscpa_t *a ) {
    uint16_t sa_out, sa_in;
    int status;
        
    /* Read value written by host */
    status = lesi_sa_read_response( &sa_in );
    if ( status )
        goto err;

    status = lesi_sa_write( sa_in );
    if ( status )
        goto err;

    return;

err:
    if ( status == ERR_INIT ) {
        a->step = STEP_REINIT;
        return;
    }
    printf("Error in diag wrap: %i, retrying\n", status);
    return;
}

void hostif_diagpp( mscpa_t *a ) {
    uint16_t sa_out, sa_in, sr;
    int status;
    
    if ( a->features & FEAT_PURGE ) {
        // The host then writes zeros to the SA register simulating a "purge completed" host action.
        /* Read value written by host */

        // Check for the PURGED status bit
        for ( ; ; ) {
            /* Read KLESI status register */
            status = lesi_read_reg( LESI_REG_STATUS, &sr );
            if ( status )
                goto err;

            if ( sr & LESI_SR_PURGED )
                break; 
        }
        printf("HostIF Purge poll test got PURGE\n");
    } else {
        printf("HostIF Purge poll skipping purge test as the KLESI does not support it\n");
    }

    // The host then reads the IP register to simulate a start polling command from the host to the port.
    for ( ; ; ) {
        /* Read KLESI status register */
        status = lesi_read_reg( LESI_REG_STATUS, &sr );
        if ( status )
            goto err;

        if ( sr & LESI_SR_POLL )
            break; 
    }

    printf("HostIF Purge poll test got POLL\n");

    status = lesi_read_reg( LESI_REG_CLEAR_POLL,   &sr );
    if ( status )
        goto err;

    status = lesi_read_reg( LESI_REG_CLEAR_PURGED, &sr );
    if ( status )
        goto err;

    // The test is complete when the controller announces the transition to step 4 in the SA register.
    a->step = 4;

    return;

err:
    if ( status == ERR_INIT ) {
        a->step = STEP_REINIT;
        return;
    }
    printf("Error in diag purge poll: %i, retrying\n", status);
    return;
}