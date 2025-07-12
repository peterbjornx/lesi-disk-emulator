#ifndef __lerror__
#define __lerror__

#define ERR_INIT      (-1&0x7F)
#define ERR_LPARITY   (-2&0x7F)
#define ERR_HPARITY   (-3&0x7F)
#define ERR_NXM       (-4&0x7F)
#define ERR_MISMATCH  (-5&0x7F)
#define ERR_BUSY      (-6&0x7F)
#define ERR_OK        (0 &0x7F)
#define ERR_BARB_TO   (-7&0x7F)
#define ERR_DATA_TO   (-8&0x7F)
#define ERR_FATAL     (0x80)

#define ERR_STATUS(S) ((S) & 0x7F)
#define ERR_WHEN(S)   ((S) & 0xFF00)

#define propagate( Status ) do { if ( Status ) return (Status); } while ( 0 )
#define propagateTagged( Status, Tag ) do { if ( Status ) return (Status) | (Tag); } while ( 0 )

#endif