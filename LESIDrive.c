#include "projconfig.h"
#include <stdio.h>
#include "lesi/lesi.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "mscp/mscp.h"
#include "blink.pio.h"

void blink_pin_forever(PIO pio, uint sm, uint offset, uint pin, uint freq) {
    blink_program_init(pio, sm, offset, pin);
    pio_sm_set_enabled(pio, sm, true);

    printf("Blinking pin %d at %d Hz\n", pin, freq);

    // PIO counter program takes 3 more cycles in total than we pass as
    // input (wait for n + 1; mov; jmp)
    pio->txf[sm] = (125000000 / (2 * freq)) - 3;
}

int lesi_selftest() {
    uint16_t rb;
    int status;
    uint16_t test_vals[19] = {1,2,4,8,0x10,0x20,0x40,0x80,0x100,0x200,0x400,0x800,0x1000,0x2000,0x4000,0x8000,0xaaaa,0x8888,0x1337};
    lesi_lowlevel_reset_klesi();
    for( int i = 0; i < 19; i++ ) {
            status = lesi_write_ram_word( 0, test_vals[i] );
            if ( status )
                return status;
            status = lesi_read_ram_word(0, &rb );
            if ( status )
                return status;
            if ( rb != test_vals[i] ) {
                printf("Selftest failed: %04X != %04X in loopback test\n", rb, test_vals[i]);
                return ERR_MISMATCH;
            }
    }
    //TODO: Test parity bit here
    lesi_lowlevel_reset_klesi();
}

uint16_t buffer[128];
uint16_t wbuf[16] = {00240, 0777, 00424, 0340, 000240, 000777};
mscpa_t mscpa;

int main()
{
    uint16_t resp;
    stdio_init_all();

    // PIO Blinking example
    /*PIO pio = pio0;
    uint offset = pio_add_program(pio, &blink_program);
    printf("Loaded program at %d\n", offset);
    
    #ifdef PICO_DEFAULT_LED_PIN
    blink_pin_forever(pio, 0, offset, PICO_DEFAULT_LED_PIN, 3);
    #else
    blink_pin_forever(pio, 0, offset, 6, 3);
    #endif*/
    // For more pio examples see https://github.com/raspberrypi/pico-examples/tree/master/pio
    lesi_lowlevel_setup();
    lesi_lowlevel_set_pwrgood(0);
    lesi_lowlevel_set_pwrgood(1);
    lesi_lowlevel_reset_klesi();
    lesi_selftest();
    //sleep_ms(5000);
    mscp_startup(&mscpa);
    int i = 0;
    while (true) {
        mscp_loop(&mscpa);
        //sleep_ms(500);
        //lesi_sa_write(i++);
        //wbuf[0] = i++; 
    //    lesi_set_host_addr( 0 );
        //lesi_write_dma( wbuf, 15 );
    } 
   /*printf( "DMA writen\n" );
   lesi_write_dma( wbuf, 16);
   printf( "SA write\n" );
   lesi_send_intr(0154, 042);
   lesi_sa_read_response(& resp);
   printf("resp: %7o",resp);*/
   for( ;; ) ;
}