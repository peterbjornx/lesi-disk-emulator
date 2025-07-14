/**
 * @file lesi/klesi.c
 * @author Peter Bosch <public@pbx.sh>
 *
 * This file implements basic actions controlling the KLESI adapter, using
 * the lesi/lowlevel.c routines for basic signalling. As a result this code
 * should be platform independent, provided that lowlevel.c is ported.
 *
 * The functionality provided here includes:
 *    * Writing to and reading from KLESI internal registers.
 *    * Writing to and reading from the KLESI scratchpad RAM.
 *    * Sending and receiving handshake values through the SA register.
 *    * Sending interrupts to the host.
 *
 * DMA functionality is built on these and is implemented in lesi/npr.c
 */

#include "lesi/lesi.h"

/**
 * Write to a single register on the KLESI card
 * @param addr Must be one of the LESI_REG_ constants
 * @param data The value to write to the register.
 * @return one of the ERR_ status codes
 */
int lesi_write_reg( int addr, uint16_t data ) {
    uint16_t cmd;
    int status;

    /* Send write register command */
    cmd  = LESI_CMD_WRITE;
    cmd |= LESI_CMD_REGSEL( addr );
    status = lesi_lowlevel_write( cmd, 1 );
    if ( status )
        return status;

    /* Send register data */
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

    /* Send read register command */
    cmd  = LESI_CMD_REGSEL( addr );
    status = lesi_lowlevel_write(  cmd, 1  );
    if ( status )
        return status;

    /* Read register data from the bus */
    return lesi_lowlevel_read( data );
}

/**
 * Some flags in the LESI STATUS register are cleared upon read,
 * however the register might be read a few times by inner routines
 * before these flags are to be handled. Hence, we preserve these
 * flags until they are read using the lesi_read_srflags()
 */
static uint16_t lesi_sticky_flags = 0;

/**
 * Read the LESI status register and clear the POLL and PURGED flags.
 *
 * @param data output pointer for the register value
 * @return one of the ERR_ status codes
 */
int lesi_read_srflags( uint16_t *data ) {
    int status;

    status = lesi_read_reg( LESI_REG_STATUS, data );
    if ( status )
        return status;

    *data |= lesi_sticky_flags;
    lesi_sticky_flags = 0;
    return ERR_OK;
}

/**
 * Read the LESI status register and preserve the POLL and PURGED flags.
 *
 * @param data output pointer for the register value
 * @return one of the ERR_ status codes
 */
int lesi_read_sr( uint16_t *data ) {
    int status;

    status = lesi_read_reg( LESI_REG_STATUS, data );
    if ( status )
        return status;

    lesi_sticky_flags |= *data & ( LESI_SR_POLL | LESI_SR_PURGED );
    return ERR_OK;

}

/**
 * Write a single word into the KLESI scratchpad RAM
 * @param addr The address in the RAM to write
 * @param data The data to write
 * @return one of the ERR_ status codes
 */
int lesi_write_ram_word( int addr, uint16_t data ) {
    uint16_t cmd;
    int status;

    /* Send the LESI write RAM command */
    cmd  = LESI_CMD_WRITE;
    cmd |= LESI_CMD_REGSEL(LESI_REG_RAM);
    cmd |= LESI_CMD_WORDCNT( addr );
    status = lesi_lowlevel_write(  cmd, 1  );
    if ( status )
        return status;

    /* Send the RAM data */
    return lesi_lowlevel_write( data, 0 );
}

/**
 * Write multiple words into the KLESI scratchpad RAM
 * @param addr The address in the RAM to write
 * @param data The data to write
 * @return one of the ERR_ status codes
 */
int lesi_write_ram( int addr, const uint16_t *data, int count ) {
    uint16_t cmd;
    int status;

    /* Send the LESI write RAM command */
    cmd  = LESI_CMD_WRITE;
    cmd |= LESI_CMD_REGSEL(LESI_REG_RAM);
    cmd |= LESI_CMD_WORDCNT( addr );
    status = lesi_lowlevel_write(  cmd, 1  ); 
    if ( status )
        return status;

    /* Send the RAM data */
    while ( count-- ) {
        status = lesi_lowlevel_write( *data++, 0 );
        if ( status )
            return status;
    }

    return ERR_OK;
}

/**
 * Read a single word from the KLESI scratchpad RAM
 * @param addr The address in the RAM to read
 * @param data The data that was read
 * @return one of the ERR_ status codes
 */
int lesi_read_ram_word( int addr, uint16_t *data ) {
    uint16_t cmd;
    int status;

    /* Send the LESI write RAM command */
    cmd  = LESI_CMD_REGSEL(LESI_REG_RAM);
    cmd |= LESI_CMD_WORDCNT( addr );
    status = lesi_lowlevel_write(  cmd, 1  );
    if ( status )
        return status;

    /* Read the RAM data */
    return lesi_lowlevel_read( data );
}

/**
 * Set the KLESI host address register
 * @param addr The host bus address to send to the adapter
 * @return one of the ERR_ status codes
 */
int lesi_set_host_addr( uint32_t addr ) {
    int status;

    /* Write the low 16 bit of the address to the UAL register */
    status = lesi_write_reg( LESI_REG_UAL, addr );
    if ( status )
        return status;

    /* Write the high bits of the address to the UAH register */
    return lesi_write_reg( LESI_REG_UAH, addr >> 16 );
}

/**
 * Polls the KLESI status register and reports any errors
 * @return Error code if an error occurred or ERR_OK.
 */
int lesi_handle_status( void ) {
    int status;
    uint16_t sr;
    
    status = lesi_read_sr( &sr );
    if ( status )
        return status;

    if ( sr & LESI_SR_BUS_PE )  /* Q/UNIBUS parity error */
        return ERR_HPARITY;
    if ( sr & LESI_SR_LESI_PE ) /* LESI parity error */
        return ERR_LPARITY;
    if ( sr & LESI_SR_NXM )     /* Tried to access non-existent memory location */
        return ERR_NXM;
    return ERR_OK;

}

/**
 * Sends an interrupt to the host and present a new value via the SA register.
 *
 * @param vector Interrupt vector to send
 * @param sa New value for the SA register
 * @return one of the ERR_ status codes
 */
int lesi_sa_intr( uint16_t vector, uint16_t sa ) {
    uint16_t buf[17];
    uint16_t cmd;
    int status;

    printf("LESI: Write SA register: %04x Vector:%04x\ns", sa, vector );

    /* Write out vector and SA register value to the scratchpad */
    buf[0] = sa;
    buf[1] = vector;
    status = lesi_write_ram( 0, buf, 2 );
    if ( status )
        return status;

    /* Enable SA register and send interrupt */
    /* This needs to be done in one go as this is one action for the adapter,
     * as normally the interrupt vector and SA register value would both
     * occupy address 0 in the KLESI scratchpad. In this special mode the SA
     * register is relegated to location 1. */
    cmd = LESI_CMD_DO_INTR | LESI_CMD_SA;
    status = lesi_lowlevel_write( cmd, 1 );
    if ( status )
        return status;

    /* Wait for the KLESI to start the operation */
    status = lesi_lowlevel_wait_busy(); //XXX: This could be a race condition, i guess
    return status;

}

/**
 * Sends an interrupt to the host.
 *
 * @param vector Interrupt vector to send
 * @return one of the ERR_ status codes
 */
int lesi_send_intr( uint16_t vector ) {
    uint16_t cmd;
    int status;

    /* Write out vector */
    status = lesi_write_ram_word( 0, vector );
    if ( status )
        return status;

    /* Send the interrupt */
    cmd = LESI_CMD_DO_INTR;
    status = lesi_lowlevel_write( cmd, 1 );
    if ( status )
        return status;

}

/**
 * Presents a word to the host via the SA register.
 *
 * The SA register is implemented by allowing the host to read from
 * KLESI scratchpad RAM location 0 while it is enabled. The host writing
 * to SA will complete this action and assert the busy signal (T1).
 *
 * @param sa New value for the SA register
 * @return one of the ERR_ status codes
 */
int lesi_sa_write( uint16_t sa ) {
    uint16_t cmd;
    int status;
    printf("LESI Write SA register: %04x\ns", sa );

    /* Write value for the SA register to the KLESI scratchpad address 0 */
    status = lesi_write_ram_word( 0, sa );
    if ( status )
        return status;

    /* Enable SA register */
    cmd = LESI_CMD_SA | LESI_CMD_CLEAR_WC;
    status = lesi_lowlevel_write( cmd, 1 );
    return status;
}

/**
 * Wait for host write to the SA register and return the value thus written.
 * @param data The new SA value from the host.
 * @return one of the ERR_ status codes
 */
int lesi_sa_read_response( uint16_t *data ) {
    int status;

    /* Wait for host to write to the SA register */
    status = lesi_lowlevel_wait_ready();
    if ( status )
        return status;

    /* Read the value the host wrote from the scratchpad */
    status = lesi_read_ram_word( 0, data );
    printf("LESI: Read SA register: %04x\ns", *data );

    return status;
}

/**
 * Wait for host write to the SA register and return the value thus written.
 * Variant function to be used when the previous transaction sent an interrupt,
 * which in some cases on KLESI-UA results in the host write being directed to
 * scratchpad location 1.
 *
 * @param data The new SA value from the host.
 * @return one of the ERR_ status codes
 */
int lesi_sa_read_response_intr( uint16_t *data ) {
    int status;

    /* Wait for host to write */
    status = lesi_lowlevel_wait_ready();
    if ( status )
        return status;

    /* Read the value the host wrote from the scratchpad */
    status = lesi_read_ram_word( 1, data );
    printf("LESI: Read SA register: %04x\ns", *data );

    return status;
}

/**
 * Disable SA register access.
 *
 * @return one of the ERR_ status codes
 */
int lesi_sa_end( void ) {
    return lesi_lowlevel_write( 0, 1 );
}

/**
 * Enable the SA register and wait for host write to the SA
 * register and return the value thus written.
 * @param data The new SA value from the host.
 * @return one of the ERR_ status codes
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