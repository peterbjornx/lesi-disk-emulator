#include "lesi/lesi.h"
#include "pico/stdlib.h"

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
static uint16_t lesi_sticky_flags = 0;
int lesi_read_srflags( uint16_t *data ) {
    int status;
    status = lesi_read_reg( LESI_REG_STATUS, data );
    propagate(status);
    *data |= lesi_sticky_flags;
    lesi_sticky_flags = 0;
    return ERR_OK;
}

int lesi_read_sr( uint16_t *data ) {
    int status;
    status = lesi_read_reg( LESI_REG_STATUS, data );
    propagate(status);
    lesi_sticky_flags |= *data & ( LESI_SR_POLL | LESI_SR_PURGED );
    return ERR_OK;

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

    return ERR_OK;
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

int lesi_set_host_addr( uint32_t addr ) {
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
    
    status = lesi_read_sr( &sr );
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


int lesi_sa_intr( uint16_t vector, uint16_t sa ) {
    uint16_t buf[17];
    uint16_t cmd;
    int status;

    /* Write out vector and SA register value */
    buf[0] = sa;
    buf[1] = vector;
    printf("LESI: Write SA register: %04x Vector:%04x\ns", sa, vector );
    status = lesi_write_ram( 0, buf, 2 );
    if ( status )
        return status;

    cmd = LESI_CMD_DO_INTR | LESI_CMD_SA;
    status = lesi_lowlevel_write( cmd, 1 );
    if ( status )
        return status;
    status = lesi_lowlevel_wait_busy();
    return status;

}


int lesi_send_intr( uint16_t vector ) {
    uint16_t cmd;
    int status;

    /* Write out vector */
    status = lesi_write_ram_word( 0, vector );
    if ( status )
        return status;

    cmd = LESI_CMD_DO_INTR;
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
    printf("LESI: Write SA register: %04x\ns", sa );
    /* Write out vector and SA register value */
    status = lesi_write_ram_word( 0, sa );
    if ( status )
        return status;
    cmd = LESI_CMD_SA | LESI_CMD_CLEAR_WC;// | LESI_CMD_REGSEL(LESI_REG_RAM);
    status = lesi_lowlevel_write( cmd, 1 );
    return status;
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

    status = lesi_read_ram_word( 0, data );
    printf("LESI: Read SA register: %04x\ns", *data );

    return status;
}
/**
 * Read SA register
 */
int lesi_sa_read_response_intr( uint16_t *data ) {
    int status;

    /* Wait for host to write */
    status = lesi_lowlevel_wait_ready();
    if ( status )
        return status;

    status = lesi_read_ram_word( 1, data );
    printf("LESI: Read SA register: %04x\ns", *data );

    return status;
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