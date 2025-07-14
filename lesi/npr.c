/**
 * @file lesi/npr.c
 * @author Peter Bosch <public@pbx.sh>
 *
 * This file implements DMA reads and writes to the host memory on top
 * of the KLESI driver routines in lesi/klesi.c
 */

#include "lesi/lesi.h"

/**
 * Reads a single 0 to 16 word block from host memory.
 * @param buffer The buffer to read the data into
 * @param count  The number of words to read from host memory.
 * @return one of the ERR_ status codes
 */
int lesi_read_dma_block( uint16_t *buffer, int count ) {
    uint16_t cmd;
    int status;

    /* Issue the start DMA command */
    cmd  = LESI_CMD_DO_NPR;
    cmd |= LESI_CMD_REGSEL(LESI_REG_RAM);
    if ( count >= 16 ) {
        cmd |= LESI_CMD_WORDCNT( 0 );
    } else {
        cmd |= LESI_CMD_WORDCNT( 16 - count );
    }
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

/**
 * Read data from host memory starting at the current host address
 * register value.
 *
 * @param buffer The buffer to read the data into.
 * @param count  The number of words to read from host memory.
 * @return one of the ERR_ status codes
 */
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

    /* Present any hardware / bus errors to the calling routine */
    return lesi_handle_status();
}

/**
 * Writes a single 0 to 16 word block of data to host memory.
 *
 * @param buffer The buffer to read the data into.
 * @param count  The number of words to write.
 * @return one of the ERR_ status codes
 */
int lesi_write_dma_block( const uint16_t *buffer, int count ) {
    uint16_t cmd;
    int status;

    /* Send the write RAM command */
    status = lesi_write_ram( 16 - count, buffer, count );
    //TODO: Why is the status ignored here
    
    /* Start the NPR via a WRITE RAM with DO NPR set */
    /* As we don't issue a data cycle, this will not write the scratchpad */
    cmd  = LESI_CMD_WRITE | LESI_CMD_DO_NPR;
    cmd |= LESI_CMD_REGSEL(LESI_REG_RAM);
    if ( count >= 16 ) {
        cmd |= LESI_CMD_WORDCNT( 0 );
    } else {
        cmd |= LESI_CMD_WORDCNT(16 - count);
    }
    status = lesi_lowlevel_write(  cmd, 1  );
    if ( status )
        return status;
    
    /* Wait for the NPR transfer to complete */
    status = lesi_lowlevel_wait_ready();
    if ( status )
        return status;

    /* Idle the KLESI */
    status = lesi_lowlevel_write(  0, 1  );
    if ( status )
        return status;

    /* Present any hardware / bus errors to the calling routine */
    return lesi_handle_status();
}

/**
 * Write data to host memory starting at the current host address
 * register value.
 *
 * @param buffer The buffer to read the data into.
 * @param count  The number of words to write.
 * @return one of the ERR_ status codes
 */
int lesi_write_dma( const uint16_t *buffer, int count ) {
    int bcount = 16, status;

    while ( count ) {
        if ( count < bcount )
            bcount = count;
        
        /* Read the block */
        status = lesi_write_dma_block( buffer, bcount );
        if ( status )
            return status;

        buffer += bcount;
        count  -= bcount;
    }

    /* Present any hardware / bus errors to the calling routine */
    return lesi_handle_status();
}

static const uint16_t zero_buf[16] = {0};

/**
 * Write zeros to host memory starting at the current host address
 * register value.
 *
 * @param count  The number of words to write.
 * @return one of the ERR_ status codes
 */
int lesi_write_dma_zeros( int count ) {
    int bcount = 16, status;

    while ( count ) {
        if ( count < bcount )
            bcount = count;
        
        /* Read the block */
        status = lesi_write_dma_block( zero_buf, bcount );
        if ( status )
            return status;

        count  -= bcount;
    }

    /* Present any hardware / bus errors to the calling routine */
    return lesi_handle_status();
}
