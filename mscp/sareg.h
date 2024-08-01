/***************************************************/
/*   Status register bit definitions               */
/***************************************************/

#define SA_ERROR          (0x8000)
#define SA_GO             (0x0001)

#define SA_ERROR_BIT      (15)

/* Initialization sequence step field*/
#define SA_INIT_STEP_BIT  (11)
#define SA_INIT_STEP_MASK (0x7800)

#define SA_INIT1_STEP     (0x0800)
#define SA_INIT2_STEP     (0x1000)
#define SA_INIT3_STEP     (0x2000)
#define SA_INIT4_STEP     (0x4000)

/***************************************************/
/*   Init sequence step 1 read bit definitions     */
/***************************************************/

/* No host inter vec settable address */
#define SA_INIT1R_NV      (0x0400)

/* 22-bit addressing support */
/* QB=1 means that the Port supports a 22-bit host bus.
 * This bit will be 0 for UNIBUS. 
 * 001000
 */
#define SA_INIT1R_QB      (0x0200)

/* Enhanced diag implementation  000400 */
#define SA_INIT1R_DI      (0x0100)

/* Port allows odd host address  000200 */
#define SA_INIT1R_OD      (0x0080)

/* Port supports address mapping 000100 */
#define SA_INIT1R_MP      (0x0040)
      
/***************************************************/
/*   Init sequence step 1 write bit definitions    */
/***************************************************/

/* Diag wrap around */
#define SA_INIT1W_WR              (0x4000)

/* Number of C-ring slots 'pwrs of 2' */
#define SA_INIT1W_CRING_MASK      (0x3800)
#define SA_INIT1W_CRING_BIT       (11)

/* Number of R-ring slots 'pwrs of 2' */
#define SA_INIT1W_RRING_MASK      (0x0700)
#define SA_INIT1W_RRING_BIT       (8)

/* Init sequence interrupt request */
#define SA_INIT1W_IE              (0x0080)

/* Interrupt vector address */
#define SA_INIT1W_VADR_MASK       (0x007F)
#define SA_INIT1W_VADR_BIT        (0)

/***************************************************/
/*   Init sequence step 2 read bit definitions     */
/***************************************************/

/* Port type number */
/* 
 0 means UNIBUS/QBUS storage systems port. (KLESI?)
*/
#define SA_INIT2R_PTYP_MASK       (0x0700)
#define SA_INIT2R_PTYP_BIT        (8)

#define SA_INIT2R_ALWAYS          (0x0080)

/* Echoed diag wrap around */
#define SA_INIT2R_WR              (0x0040)

/* Echoed C-ring */
#define SA_INIT2R_CRING_MASK      (0x0038)
#define SA_INIT2R_CRING_BIT       (3)

/* Echoed R-ring */
#define SA_INIT2R_RRING_MASK      (0x0007)
#define SA_INIT2R_RRING_BIT       (0)

/***************************************************/
/*   Init sequence step 2 write bit definitions    */
/***************************************************/

/* Ring base lower address */
#define SA_INIT2W_RINGBASE_MASK   (0xFFFE)
#define SA_INIT2W_RINGBASE_BIT    (0)

/* Adapter purge interrupt request */
#define SA_INIT2W_PI              (0x0001)

/***************************************************/
/*   Init sequence step 3 read bit definitions     */
/***************************************************/

/* Echoed IE bits from INIT1W */
#define SA_INIT3R_IE              (0x0080)

/* Echoed vector address */
#define SA_INIT3R_VADR_MASK       (0x007F)
#define SA_INIT3R_VADR_BIT        (0)

/***************************************************/
/*   Init sequence step 3 write bit definitions    */
/***************************************************/

/* Purge and poll test request*/
#define SA_INIT3W_PP              (0x8000)

/* Ring base high address */
#define SA_INIT3W_HRBASE_MASK     (0x7FFF)
#define SA_INIT3W_HRBASE_BIT      (0)

/***************************************************/
/*   Init sequence step 4 read bit definitions     */
/***************************************************/

/* Encoded controller identification */
#define SA_INIT4R_MOD_MASK        (0x00F0)
#define SA_INIT4R_MOD_BIT         (4)

/* EMod 16 value of u-code version */
#define SA_INIT4R_VER_MASK        (0x000F)
#define SA_INIT4R_VER_BIT         (0)


/***************************************************/
/*   Init sequence step 4 write bit definitions    */
/***************************************************/

/* Max number of longwords per NPR xfer */
#define SA_INIT4W_BURST_MASK      (0xFF00)
#define SA_INIT4W_BURST_BIT       (8)

/* Last fail request*/
#define SA_INIT4W_LF              (0x0002)

/* Go bit */
#define SA_INIT4W_GO              (0x0001)