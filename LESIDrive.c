#include "projconfig.h"
#include <stdio.h>
#include <mscp/server/server.h>

#include "driver/usbmsc.h"
#include "lesi/lesi.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "mscp/mscp.h"

#define AIRCR_Register (*((volatile uint32_t*)(PPB_BASE + 0x0ED0C)))

int lesi_selftest() {
    uint16_t rb;
    int status;
    uint16_t test_vals[19] = {1,2,4,8,0x10,0x20,0x40,0x80,0x100,0x200,0x400,0x800,0x1000,0x2000,0x4000,0x8000,0xaaaa,0x8888,0x1337};

    /* Reset the KLESI adapter */
    lesi_lowlevel_reset_klesi();

    /* Loopback test to KLESI scratchpad RAM */
    for( int i = 0; i < 19; i++ ) {
        status = lesi_write_ram_word( 0, test_vals[i] );
        if ( status )
            return status;
        status = lesi_read_ram_word(0, &rb );
        if ( status )
            return status;
        if ( rb != test_vals[i] ) {
            printf("Selftest failed: %04X != %04X in loopback test\n", rb, test_vals[i]);
           // return ERR_MISMATCH;
        }
    }
    //TODO: Test parity bit here

    /* Reset the KLESI adapter */
    lesi_lowlevel_reset_klesi();
    return ERR_OK;
}

mscpa_t *hostif;
mscps_t *server;

int main()
   {
    stdio_init_all();

    lesi_lowlevel_setup();
    lesi_lowlevel_set_pwrgood(0);
    lesi_lowlevel_set_pwrgood(1);
    lesi_lowlevel_reset_klesi();
    lesi_selftest();
    hostif = hostif_setup();
    server = mscps_setup();
    mscps_attach( server, hostif );
    usbmsc_init(server, 0);

    /* Attempt to get the USB drive mounted */
    for ( int i = 0; i < 1000; i++ )
    {
        usbmsc_process();
        sleep_ms(1);
        if ( server->c_unit->u_state == MUS_AVAIL)
            break;
    }

    /* If it failed, pull our reset line */
    if ( server->c_unit->u_state != MUS_AVAIL) {
        sleep_ms(10);
        AIRCR_Register = 0x5FA0004;
    }

    /* Main loop */
    while (true) {
        hostif_loop( hostif );
        mscps_loop( server );
        usbmsc_process();
    }
}

void app_idle() {
    usbmsc_process();
}