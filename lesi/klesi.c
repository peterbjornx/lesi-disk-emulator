#include "lesi/lesi.h"
#include "pico/stdlib.h"

/* VECTOR forces RAM address 1? */

/**
 * Write to a single register on the KLESI card
 * @param addr Must be one of the LESI_REG_ constants
 * @param data The value to write to the register.
 */
int lesi_write_reg( int addr, uint16_t data ) {
    uint16_t cmd;
    int status;
    cmd  = LESI_CMD_WRITE;
    cmd |= LESI_CMD_REGSEL( addr );
    status = lesi_lowlevel_write( cmd, 1 );
    if ( status )
        return status;
    return lesi_lowlevel_write( data, 0 );
}

/**
 * Reads from a single register on the KLESI card
 * @param addr Must be one of the LESI_REG_ constants
 * @param data output pointer for the register value
 * @return one of the ERR_ status codes
 */
int lesi_read_reg( int addr, uint16_t *data ) {
    uint16_t cmd;
    int status;
    cmd  = 0;
    cmd |= LESI_CMD_REGSEL( addr );
    status = lesi_lowlevel_write(  cmd, 1  );
    if ( status )
        return status;
    return lesi_lowlevel_read( data );
}

int lesi_write_ram_word( int addr, uint16_t data ) {
    uint16_t cmd;
    int status;
    cmd  = LESI_CMD_WRITE;
    cmd |= LESI_CMD_REGSEL(LESI_REG_RAM);
    cmd |= LESI_CMD_WORDCNT( addr );
    status = lesi_lowlevel_write(  cmd, 1  );
    if ( status )
        return status;
    return lesi_lowlevel_write( data, 0 );
}

int lesi_write_ram( int addr, const uint16_t *data, int count ) {
    uint16_t cmd;
    int status;
    cmd  = LESI_CMD_WRITE;
    cmd |= LESI_CMD_REGSEL(LESI_REG_RAM);
    cmd |= LESI_CMD_WORDCNT( addr );
    status = lesi_lowlevel_write(  cmd, 1  ); 
    if ( status )
        return status;
    while ( count-- ) {
        status = lesi_lowlevel_write( *data++, 0 );
        if ( status )
            return status;
    }
}

int lesi_read_ram_word( int addr, uint16_t *data ) {
    uint16_t cmd;
    int status;
    cmd  = 0;
    cmd |= LESI_CMD_REGSEL(LESI_REG_RAM);
    cmd |= LESI_CMD_WORDCNT( addr );
    status = lesi_lowlevel_write(  cmd, 1  );
    if ( status )
        return status;
    return lesi_lowlevel_read( data );
}

int     lesi_set_host_addr( uint32_t addr ) {
    int status;
    status = lesi_write_reg( LESI_REG_UAL, addr );
    if ( status )
        return status;
    return lesi_write_reg( LESI_REG_UAH, addr >> 16 );
}

/**
 * Polls the KLESI status register and reports any errors
 * @return Error code if an error occurred or ERR_OK.
 */
int lesi_handle_status( void ) {
    int status;
    uint16_t sr;
    
    status = lesi_read_reg( LESI_REG_STATUS, &sr );
    if ( status )
        return status;
    if ( sr & LESI_SR_BUS_PE )
        return ERR_HPARITY;
    if ( sr & LESI_SR_LESI_PE )
        return ERR_LPARITY;
    if ( sr & LESI_SR_NXM )
        return ERR_NXM;
    return ERR_OK;

}

int lesi_read_dma_block( uint16_t *buffer, int count ) {
    uint16_t cmd, bcount, sr;
    int status;
    cmd  = LESI_CMD_DO_NPR;
    cmd |= LESI_CMD_REGSEL(LESI_REG_RAM);
    if ( count >= 16 ) {
        cmd |= LESI_CMD_WORDCNT( 0 );
    } else {
        cmd |= LESI_CMD_WORDCNT(16 - count);
    }
    
    /* Issue the DO NPR command */
    status = lesi_lowlevel_write(  cmd, 1  );
    if ( status )
        return status;

    /* Wait for the NPR transaction to complete */
    status = lesi_lowlevel_wait_ready();
    if ( status )
        return status;
    
    /* Stop the NPR from being reissued as we read out the data */
    cmd &= ~LESI_CMD_DO_NPR;
    status = lesi_lowlevel_write(  cmd, 1  );
    if ( status )
        return status;
    
    while ( count-- ) {
        /* Accept a word of data from the KLESI RAM */
        status = lesi_lowlevel_read( buffer++ );
        if ( status )
            return status;
        
        /* Request the next word of data */
        if ( count ) {
            status = lesi_lowlevel_read_strobe( 0 );
            if ( status )
                return status;
        }
    }

    return ERR_OK;
}

int lesi_read_dma( uint16_t *buffer, int count ) {
    int bcount = 16, status;

    while ( count ) {
        if ( count < bcount )
            bcount = count;
        
        /* Read the block */
        status = lesi_read_dma_block( buffer, bcount );
        if ( status )
            return status;

        buffer += bcount;
        count  -= bcount;
    }
    return lesi_handle_status();
}

int lesi_read_dma( uint16_t *buffer, int count ) {
    uint16_t cmd, bcount, sr;
    int status;
    cmd  = LESI_CMD_DO_NPR;
    cmd |= LESI_CMD_REGSEL(LESI_REG_RAM);
    if ( count >= 16 ) {
        cmd |= LESI_CMD_WORDCNT( 0 );
    } else {
        cmd |= LESI_CMD_WORDCNT(16 - count);
    }
    
    status = lesi_lowlevel_write(  cmd, 1  );
    if ( status )
        return status;

    while ( count > 0 ) {
        //TODO: Check status
        bcount = count;
        if ( bcount > 16 )
            bcount = 16;
        count -= bcount;
        status = lesi_lowlevel_wait_ready();
        if ( status )
            return status;
        if ( count < 16 ) {
            // Last xfer, stop DMA
            cmd &= ~LESI_CMD_DO_NPR;
            status = lesi_lowlevel_write(  cmd, 1  );
            if ( status )
                return status;
        }
        while ( bcount-- ) {
            status = lesi_lowlevel_read( buffer++ );
            if ( status )
                return status;
            if ( bcount ) {
                status = lesi_lowlevel_read_strobe( 0 );
                if ( status )
                    return status;
            }
            if ( status )
                return status;
        }
        if ( count > 0 && count < 16 ) {   
            cmd |=  LESI_CMD_DO_NPR;
            cmd |=  LESI_CMD_WORDCNT(16 - count);
            status = lesi_lowlevel_write(  cmd, 1  );
            if ( status )
                return status;
        } else if ( count > 0 ) {
            status = lesi_lowlevel_write(  cmd, 1  );
            if ( status )
                return status;
        }

    }

    return lesi_handle_status();
}

int lesi_write_dma( uint16_t *buffer, int count ) {
    uint16_t cmd, bcount, start = 1, sr;
    int status;
    cmd  = LESI_CMD_WRITE;
    cmd |= LESI_CMD_REGSEL(LESI_REG_RAM);
    if ( count >= 16 ) {
        cmd |= LESI_CMD_WORDCNT( 0 );
    } else {
        cmd |= LESI_CMD_WORDCNT(16 - count);
    }
    status = lesi_lowlevel_write(  cmd, 1  );
    if ( status )
        return status;
    while ( count > 0 ) {
        //TODO: Check status
        bcount = count;
        if ( bcount > 16 )
            bcount = 16;
        count -= bcount;
        while ( bcount-- ) {
            status = lesi_lowlevel_write( *buffer++, 0 );
            if ( status )
                return status;
        }
        if ( start ) {
            cmd |=  LESI_CMD_DO_NPR;
            status = lesi_lowlevel_write(  cmd, 1  );
            if ( status )
                return status;
           // start = 0;
        }
        status = lesi_lowlevel_wait_ready();
        if ( status )
            return status;
        if ( 1 ) { //count < 16 ) {
            // Last xfer, stop DMA
            cmd = LESI_CMD_WRITE;
            cmd &= ~LESI_CMD_DO_NPR;
            if ( count <= 0 )
                cmd &= ~LESI_CMD_WRITE;
            status = lesi_lowlevel_write(  cmd, 1  );
            if ( status )
                return status;
        }
        if ( count > 0 && count < 16 ) {      
            cmd |=  LESI_CMD_WORDCNT(16 - count);
            status = lesi_lowlevel_write(  cmd, 1  );
            if ( status )
                return status;
            start = 1;
        }
    }

    return lesi_handle_status();
}

int lesi_write_dma_zeros( int count ) {
    uint16_t cmd, bcount, start = 1, sr;
    int status;
    cmd  = LESI_CMD_WRITE;
    cmd |= LESI_CMD_REGSEL(LESI_REG_RAM);
    if ( count >= 16 ) {
        cmd |= LESI_CMD_WORDCNT( 0 );
    } else {
        cmd |= LESI_CMD_WORDCNT(16 - count);
    }
    status = lesi_lowlevel_write(  cmd, 1  );
    if ( status )
        return status;
    while ( count > 0 ) {
        //TODO: Check status
        bcount = count;
        if ( bcount > 16 )
            bcount = 16;
        count -= bcount;
        while ( bcount-- ) {
            status = lesi_lowlevel_write( 0, 0 );
            if ( status )
                return status;
        }
        if ( start ) {
            cmd |=  LESI_CMD_DO_NPR;
            status = lesi_lowlevel_write(  cmd, 1  );
            if ( status )
                return status;
           // start = 0;
        }
        status = lesi_lowlevel_wait_ready();
        if ( status )
            return status;
        if ( 1 ) {//count <= 16 ) {
            // Last xfer, stop DMA
            cmd = LESI_CMD_WRITE;
            cmd &= ~LESI_CMD_DO_NPR;
            if ( count <= 0 )
                cmd &= ~LESI_CMD_WRITE;
            status = lesi_lowlevel_write(  cmd, 1  );
            if ( status )
                return status;
        }
        if ( count > 0 && count < 16 ) {      
            cmd |=  LESI_CMD_WORDCNT(16 - count);
            status = lesi_lowlevel_write(  cmd, 1  );
            if ( status )
                return status;
            start = 1;
        }
    }

    return lesi_handle_status();
}


int lesi_send_intr( uint16_t vector, uint16_t sa ) {
    uint16_t buf[17];
    uint16_t cmd;
    int status;

    /* Write out vector and SA register value */
    buf[0] = sa;
    buf[1] = vector;
    status = lesi_write_ram( 0, buf, 2 );
    if ( status )
        return status;

    cmd = LESI_CMD_DO_INTR | LESI_CMD_SA;
    status = lesi_lowlevel_write( cmd, 1 );
    if ( status )
        return status;

}

/**
 * Write to RAM 0 (SA) and enable SA register.
 */
int lesi_sa_write( uint16_t sa ) {
    uint16_t cmd;
    int status;

    /* Write out vector and SA register value */
    status = lesi_write_ram_word( 0, sa );
    if ( status )
        return status;
    cmd = LESI_CMD_SA | LESI_CMD_CLEAR_WC;// | LESI_CMD_REGSEL(LESI_REG_RAM);
    return lesi_lowlevel_write( cmd, 1 );
}

/**
 * Read SA register
 */
int lesi_sa_read_response( uint16_t *data ) {
    int status;

    /* Wait for host to write */
    status = lesi_lowlevel_wait_ready();
    if ( status )
        return status;

    return lesi_read_ram_word( 0, data );
}

/**
 * Disable SA register access
 */
int lesi_sa_end( void ) {
    return lesi_lowlevel_write( 0, 1 );
}

/**
 * Read SA register
 */
int lesi_sa_read( uint16_t *data) {
    uint16_t cmd;
    int status;

    /* Enable SA */
    cmd = LESI_CMD_SA;
    status = lesi_lowlevel_write( cmd, 1 );
    if ( status )
        return status;

    /* Wait for host to write */
    return lesi_sa_read_response( data );
}