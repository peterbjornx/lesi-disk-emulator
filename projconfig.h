/* Controller version */
#define MSCP_HW_VERSION    (1)
#define MSCP_FW_VERSION    (1)

/* Port configuration */
#define MSCP_MOD_ID        MODEL_ID_RC25
#define MSCP_PORT_TYPE     PORT_TYPE_QBUS_UNIBUS
#define MSCP_BASE_FEATURES (FEAT_VEC | FEAT_ENH_DIAG ) /* ZRCFA requires ENH DIAG */
#define MSCP_DEF_BURSTSZ   (8)

/* Controller identity */

#define MSCP_CID_CLASS     (M_CC_MASS)
#define MSCP_CID_MODEL     (M_CM_UDA50)
#define MSCP_CID_UIDH      (0x42)
#define MSCP_CID_UIDL      (0x13371337)

#define MSCP_CFLAGS        (0)
#define MSCP_CFLAGMASK     (0)

#define MSCP_CUNITS        (2)

#undef USBMSC_ENA