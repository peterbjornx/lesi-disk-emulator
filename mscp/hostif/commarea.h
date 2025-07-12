typedef struct __attribute__((packed)) {
    uint16_t rsvd; // 0
    uint8_t  rsvdb; // 2
    uint8_t  adapter_ch; // 3
    uint16_t cmd_int; // 4
    uint16_t rsp_int; // 6
} hostif_cahdr_t;

typedef uint32_t hostif_desc_t;

#define MSCP_DESC_OWNER  (1<<31)
#define MSCP_DESC_FLAG   (1<<30)
#define MSCP_DESC_18ADDR_MASK  (0x0003FFFF)
#define MSCP_DESC_22ADDR_MASK  (0x003FFFFF)

typedef struct __attribute__((packed)) {
    uint16_t    msg_len;
    uint8_t     type_credits;
    uint8_t     conn_id;
} hostif_envhdr_t;

#define MSCP_MSGTYPE_SEQ    (0)
#define MSCP_MSGTYPE_DGRAM  (1)
#define MSCP_MSGTYPE_CREDIT (2)
#define MSCP_MSGTYPE_MAINT  (15)