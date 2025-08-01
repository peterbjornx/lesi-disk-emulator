/**
 * @file lesi/lowlevel.c
 * @author Peter Bosch <public@pbx.sh>
 *
 * This file implements the lowest level logic for communicating with
 * the KLESI adapter. As a result it is one of the main areas of focus when
 * porting the emulator to different microcontroller platforms.
 *
 * As a rule of thumb, the functionality implemented here wraps electrical
 * signalling via GPIO pins into more understandable functions, but does
 * not implement any bus logic beyond basic timing.
 *
 */
#include <hardware/watchdog.h>
#include <pico/stdlib.h>

#include "lesi/hwconfig.h"
#include "lesi/lesi.h"

void app_idle();

/* Convenience definitions */
#define TRUE_L             (0)
#define FALSE_H            (0)
#define TRUE_H             (1)
#define FALSE_L            (1)
#define LESI_CMD_MASK      (1<<LESI_CMD_PIN)
#define LESI_STROBE_MASK   (1<<LESI_STROBE_PIN)
#define LESI_DATA_MASK     (0xFFFF << LESI_D0_PIN)
#define LESI_PAR_MASK      (3 << LESI_PAR_PIN)
#define LESI_DATA_PAR_MASK (LESI_PAR_MASK|LESI_DATA_MASK)

#define LESI_DIR_WRITE (1)
#define LESI_DIR_READ  (0)
#define NS_PER_CLK (10)

/**
 * Helper routine to suspend execution shorter than the minimum
 * duration supported by the SDK
 */
void __attribute__((optimize("O0"))) wait_ns(int amount) {
    int i =0;
    amount /= NS_PER_CLK;
    for (i = 0; i < amount;i++) {
        asm("nop");
    };
}

/* Parity helpers */
static inline uint8_t pareven8(uint8_t v)
{
    return (0x6996u >> ((v ^ (v >> 4)) & 0xf)) & 1;
}

static inline uint8_t parhilo( uint16_t v ) {
    return (pareven8(v) | (pareven8(v >> 8) << 1));
} 

/* GPIO helpers */

static int lesi_bus_dir = -1;

/**
 * Switch over LESI bus IO direction
 * Note: no turnaround delay is used
 */
static inline void lesi_bus_data_dir ( int write ) { 
    if ( lesi_bus_dir == write )
        return;

    // If going to read, first switch over GPIO
    if ( !write )
        gpio_set_dir_in_masked( LESI_DATA_PAR_MASK );
    
    gpio_put( LESI_BUF_WRITE_PIN, write );

    // If going to write, switch GPIO after buffer
    if ( write ) 
        gpio_set_dir_out_masked( LESI_DATA_PAR_MASK );
    
    wait_ns( LESI_DELAY_TURNAROUND );
    
    lesi_bus_dir = write;

    
}

/**
 * Read value currently on LESI C/D bus, verifying parity bits.
 * @param data Output pointer for the data thus read.
 * @return Error code ( ERR_OK, ERR_LPARITY )
 */
static inline int lesi_bus_read( uint16_t *data ) {
    uint32_t datin;
    uint32_t par, parexp;

    datin = gpio_get_all() & LESI_DATA_PAR_MASK;
    par   = ((datin >> LESI_PAR_PIN)) & 3u;
    *data  = (~(datin >> LESI_D0_PIN)) & 0xFFFFu;
    parexp = parhilo( *data );

    if (parexp != par )
        return ERR_LPARITY;

    return ERR_OK;
}


/* Low level actions */

volatile int saw_init = 0;

/**
 * ISR for LESI INIT L state change interrupt
 */
void lesi_init_irq(uint gpio, uint32_t event_mask) {
    printf("Init received!\n");
    saw_init = 1;
}

/**
 * Handle INIT request and clear INIT flag.
 */
void lesi_clear_init() {

    /* Wait for INIT to be deasserted */
    while (gpio_get(LESI_INIT_PIN));

    /* Clear INIT flag */

    //XXX: Reset emulator firmware to work around logic bugs
    saw_init = 0;
#define AIRCR_Register (*((volatile uint32_t*)(PPB_BASE + 0x0ED0C)))

    AIRCR_Register = 0x5FA0004;
}

/**
 * Check whether a LESI INIT was requested
 * @return 0 if no INIT was seen.
 */
int lesi_check_init() {
    return saw_init;
}

/**
 * Configure all needed GPIO pads and register INIT interrupt
 */
void lesi_lowlevel_setup() {
    int i;
    for ( i = 0; i < 16; i++) {
        gpio_init( LESI_D0_PIN + i );
        gpio_set_dir( LESI_D0_PIN + i, GPIO_IN );
    }
    for ( i = 0; i < 2; i++) {
        gpio_init( LESI_PAR_PIN + i );
        gpio_set_dir( LESI_PAR_PIN + i, GPIO_IN );
    }
    gpio_init( LESI_INIT_PIN     );
    gpio_init( LESI_T1_PIN       );
    gpio_init( LESI_AC_CLEAR_PIN );
    gpio_init( LESI_STROBE_PIN   );
    gpio_init( LESI_CP_OK_PIN    );
    gpio_init( LESI_CMD_PIN      );
    gpio_init( LESI_BUF_OE_PIN    );
    gpio_init( LESI_BUF_WRITE_PIN      );
    gpio_set_dir( LESI_INIT_PIN     , GPIO_IN );
    gpio_set_dir( LESI_T1_PIN       , GPIO_IN );
    gpio_set_dir( LESI_AC_CLEAR_PIN , GPIO_OUT );
    gpio_set_dir( LESI_STROBE_PIN   , GPIO_OUT );
    gpio_set_dir( LESI_CP_OK_PIN    , GPIO_OUT );
    gpio_set_dir( LESI_CMD_PIN      , GPIO_OUT );
    gpio_set_dir( LESI_BUF_WRITE_PIN    , GPIO_OUT );
    gpio_set_dir( LESI_BUF_OE_PIN      , GPIO_OUT );
    gpio_put( LESI_BUF_OE_PIN, 0 );
    gpio_put( LESI_BUF_WRITE_PIN, 0 );
    gpio_put( LESI_STROBE_PIN, 0 );
    gpio_put( LESI_CMD_PIN, 0 );
    gpio_put( LESI_CP_OK_PIN , 0 );
    lesi_bus_data_dir( LESI_DIR_READ );

    /* 
    There is no guaranteed pulse width for the INIT signal,
    as such, we need to use a pin change interrupt to recognize its
    rising edge 
    */
    gpio_set_irq_enabled_with_callback( LESI_INIT_PIN, GPIO_IRQ_EDGE_RISE, 1, lesi_init_irq );
}

/**
 * Issue a write cycle on the LESI bus.
 * @param data The data to drive on LESI C/D
 * @param cmd  Whether this should be a command cycle.
 * @return Status code.
 */
int lesi_lowlevel_write( uint16_t data, int cmd ) {
    uint32_t data_par, par;

    /* Parity */
    par = parhilo( data );

    /* Pack data and parity */
    data_par  = LESI_DATA_MASK & ~data;
    data_par |= LESI_PAR_MASK  & (par << LESI_PAR_PIN);

    /* Assert COMMAND L */
    if ( cmd ) {
        gpio_set_mask( 1 << LESI_CMD_PIN );
        wait_ns( LESI_DELAY_CMD_STROBE );
    }

    /* Switch bus to output and present data */
    lesi_bus_data_dir( LESI_DIR_WRITE );
    gpio_put_masked( LESI_DATA_PAR_MASK, data_par );

    /* Strobe data into KLESI */
    wait_ns( LESI_DELAY_WR_SETUP );
    gpio_set_mask( LESI_STROBE_MASK );
    wait_ns( LESI_DELAY_WR_STROBE );
    gpio_clr_mask( LESI_STROBE_MASK );

    /* Deassert COMMAND L */
    if ( cmd ) {
        wait_ns( LESI_DELAY_STROBE_CMD );
        gpio_clr_mask( 1 << LESI_CMD_PIN );
        lesi_bus_data_dir( LESI_DIR_READ );
        wait_ns( LESI_DELAY_CMD_END );
    }

    if ( saw_init )
        return ERR_INIT;

#ifdef CFG_DBG_LESI_IO
    printf("LESI WRITE: %07o [%i] ", data, cmd);
    if ( cmd ) {
        printf("WC=%i, REG=%i",LESI_CMD_WORDCNT(data),LESI_CMD_REGSEL_R(data));
        if ( data & LESI_CMD_WRITE)
            printf(" WRITE");
        if ( data & LESI_CMD_BYTE )
            printf(" BYTE");
        if ( data & LESI_CMD_CLEAR_WC)
            printf(" CLR_WC");
        if ( data & LESI_CMD_DO_NPR)
            printf(" NPR");
        if ( data & LESI_CMD_DO_INTR)
            printf(" INTR");
        if ( data & LESI_CMD_SA)
            printf(" SA");
    }
    printf("\n");
#endif

    return ERR_OK;
}

/**
 * Read from the LESI bus.
 * @param data The data read from the bus.
 * @return Status code.
 */
int lesi_lowlevel_read( uint16_t *data ) {
    int status;

    /* Turnaround bus */
    lesi_bus_data_dir( LESI_DIR_READ );
    
    /* Grab the data */
    status = lesi_bus_read( data );

    if ( saw_init )
        return ERR_INIT;

    if ( status )
        return status;

    return ERR_OK;
}

/**
 * Strobe the LESI bus for a read
 * @param waitxfer If this is set, the call will block with STROBE asserted until T1 asserts.
 * @return Status code.
 */
int lesi_lowlevel_read_strobe( int waitxfer ) {
    int status;
    
    if ( saw_init )
        return ERR_INIT;

    /* Assert strobe */
    gpio_set_mask( LESI_STROBE_MASK );

    /* Wait for the status signal to become valid */
    wait_ns( LESI_DELAY_RD_STROBE );

    if ( waitxfer ) {
        status = lesi_lowlevel_wait_ready();
        if ( status )
            return status;
    }

    gpio_clr_mask( LESI_STROBE_MASK );

    if ( saw_init )
        return ERR_INIT;

    return ERR_OK;
}

/**
 * Wait for LESI T1 to be asserted (i.e. for the KLESI to become ready).
 * @return Status code.
 */
int lesi_lowlevel_wait_ready() {
#ifdef CFG_DBG_LESI_IO
    printf("LESI WAIT....");
#endif
    busy_wait_us(50);
    for ( ;; ) {
        if ( gpio_get(LESI_T1_PIN) )
            return ERR_OK;
        if ( saw_init )
            return ERR_INIT;
        app_idle();
    }
#ifdef CFG_DBG_LESI_IO
    printf("DONE\n");
#endif
    //TODO: A better version of this must be possible
}

/**
 * Wait for LESI T1 to be deasserted (i.e. for the KLESI to become busy).
 * @return  Status code.
 */
int lesi_lowlevel_wait_busy() {
    uint16_t pin;
#ifdef CFG_DBG_LESI_IO
    printf("LESI WAIT....");
#endif
    //busy_wait_us(50);
    for ( ;; ) {
        if ( !gpio_get(LESI_T1_PIN) )
            return ERR_OK;
        if ( saw_init )
            return ERR_INIT;
        app_idle();
    }
#ifdef CFG_DBG_LESI_IO
    printf("DONE\n");
#endif
    //TODO: A better version of this must be possible
}

/**
 * Sets the controller power good signal.
 */
void lesi_lowlevel_set_pwrgood( int good ) {
    gpio_put( LESI_CP_OK_PIN, good );
    busy_wait_us_32( LESI_DELAY_PWRGOOD );
}

/**
 * Sends a reset pulse to the KLESI card
 */
void lesi_lowlevel_reset_klesi() { 
    gpio_set_mask( 1 << LESI_AC_CLEAR_PIN );
    busy_wait_us_32( LESI_DELAY_AC_CLEAR );
    gpio_clr_mask( 1 << LESI_AC_CLEAR_PIN );
    busy_wait_us_32( LESI_DELAY_AC_CLEAR );
}