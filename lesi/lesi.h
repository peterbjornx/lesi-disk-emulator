#ifndef _LESIH_
#define _LESIH_
#include <stdint.h>
#include <stdio.h>
#include "error.h"

void lesi_lowlevel_setup();

int  lesi_lowlevel_write( uint16_t data, int cmd );

int  lesi_lowlevel_read( uint16_t *data );

int lesi_lowlevel_read_strobe( int waitxfer );

int  lesi_lowlevel_wait_ready();
int  lesi_lowlevel_wait_busy();

void lesi_lowlevel_set_pwrgood( int good );

void lesi_lowlevel_reset_klesi();

void lesi_clear_init();
int lesi_check_init();

/* KLESI Command register definitions */
#define LESI_CMD_WORDCNT(i)   ((i & 15)<<0)
#define LESI_CMD_BYTE         (0x0080)      /* 000200 */
#define LESI_CMD_REGSEL(i)    ((i & 7)<<8)
#define LESI_CMD_REGSEL_R(i)  ((i >> 8)&7)
#define LESI_CMD_DO_NPR       (0x0800)      /* 004000 */
#define LESI_CMD_DO_INTR      (0x1000)      /* 010000 */
#define LESI_CMD_WRITE        (0x2000)      /* 020000 */
/* While enabled this allows the host to access RAM through the SA register */
#define LESI_CMD_SA           (0x4000)      /* 040000 */
#define LESI_CMD_CLEAR_WC     (0x8000)      /* 100000 */

// 0.000. 000.0 00.00 0.000

#define LESI_REG_RAM          (0)
#define LESI_REG_STATUS       (1) /* Read only */
#define LESI_REG_UAL          (1) /* Write only */
#define LESI_REG_UAH          (2) /* Write only */
#define LESI_REG_CLEAR_POLL   (2) /* Command, "read" only */
#define LESI_REG_CLEAR_PURGED (3) /* Command, "read" only, UNIBUS only */


/* KLESI Status register definitions */
/** KLESI Identification  */
#define LESI_SR_IDENT_UNIBUS  (0x0001)
#define LESI_SR_IDENT_QBUS    (0x0002)
//TODO: What is the ident of the VAXBI KLESI-B?
#define LESI_SR_IDENT_MASK    (0x0007)
#define LESI_SR_POLL          (0x0008)
#define LESI_SR_PURGED        (0x0010) /* UNIBUS only, fixed 1 on QBUS */
/** Parity error occurred on the host bus */
#define LESI_SR_BUS_PE        (0x0020)
/** Parity error occurred on the LESI bus */
#define LESI_SR_LESI_PE       (0x0040)
/** DMA hit non-existent memory condition */
#define LESI_SR_NXM           (0x0080)

int  lesi_write_reg( int addr, uint16_t data );
int  lesi_read_reg ( int addr, uint16_t *data );

int lesi_read_srflags( uint16_t *data );
int lesi_read_sr( uint16_t *data );

int lesi_write_ram_word( int addr, uint16_t data );
int lesi_write_ram( int addr, const uint16_t *data, int count );
int lesi_read_ram_word( int addr, uint16_t *data );

int lesi_set_host_addr( uint32_t addr );

int lesi_handle_status( void );

int lesi_read_dma( uint16_t *buffer, int count );
int lesi_write_dma( uint16_t *buffer, int count );
int lesi_write_dma_zeros( int count );

int lesi_send_intr( uint16_t vector);
int lesi_sa_intr  ( uint16_t vector, uint16_t status );
int lesi_sa_write ( uint16_t sa );
int lesi_sa_read  ( uint16_t *data ); 
int lesi_sa_end   ( void ) ;
int lesi_sa_read_response( uint16_t *data );
int lesi_sa_read_response_intr( uint16_t *data );

#endif