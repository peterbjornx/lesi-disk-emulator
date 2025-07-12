/*
 * Copyright (c) 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)mscp.h	1.2 (2.11BSD) 1998/1/28
 */

/*
 * Definitions for the Mass Storage Control Protocol
 *
 *	Unused mscp definitions are commented out because of the
 *	limited symbol table size in the V7 C compiler,
 *	`too many defines'.
 *	Lots of strangeness in mscp packet definitions, i.e.,
 *	dealing with longs as hi & lo words. This is due to the
 *	way the PDP-11 stores a long, i.e., high order word
 *	in low memory word and low order word in high memory word.
 *	The UDA does not like that at all !
 *
 *	Fred Canter 10/22/83
*/
#ifndef __mscppkt__
#define __mscppkt__

#include <stdint.h>
#include "mscp/opcode.h"

typedef uint16_t u_short;
typedef uint64_t quad;
typedef uint8_t  u_char;


typedef struct __attribute__((packed)) mscp_id {
	uint32_t i_uid_l;
	uint16_t i_uid_h;
	uint8_t  i_model;
	uint8_t  i_class;
} mscp_id_t;

/*
 * An MSCP packet
 */

struct __attribute__((packed)) mscp_pkt {
	uint32_t m_cmdref;		/* command reference number */
	u_short	m_unit;			/* unit number */ //4
	u_short	m_xxx1;			/* unused */ 
	u_char	m_opcode;		/* opcode *///8
	u_char	m_flags;		/* end message flags */
	u_short	m_modifier;		/* modifiers */
	union {// 12
	struct {
		long	Ms_bytecnt;	/* byte count */
		long  	Ms_buf; 	/* buffer descriptor hi  word */
		long	Ms_xx2[2];	/* unused */
		long    Ms_lba;	    /* logical bhock number hi  word */
		long	Ms_xx4;		/* unused */
		long	*Ms_dscptr;	/* pointer to descriptor (software) */
		long	Ms_sftwds[4];	/* software words, padding */
	} m_generic;
	struct {
		u_short	Ms_version;	/* MSCP version */
		u_short	Ms_cntflgs;	/* controller flags */
		u_short	Ms_hsttmo;	/* host timeout */
		u_short	Ms_usefrac;	/* use fraction */
		long	Ms_time;	/* time and date */
	} m_setcntchar;
	struct {
		u_short	Ms_rsvd;	/* MSCP version */
		u_short	Ms_unitflgs;/* unit flags */
		 long  Ms_rsvd2[3];
		uint32_t Ms_ddp;
	} m_online;
	struct {
		uint32_t Ms_orn;
	} m_abort;
	struct {
		u_short	Ms_multunt;	/* multi-unit code */
		u_short	Ms_unitflgs;	/* unit flags */
		long	Ms_hostid;	/* host identifier */
		quad	Ms_unitid;	/* unit identifier */
		long	Ms_mediaid;	/* media type identifier */
		u_short	Ms_shdwunt;	/* shadow unit */
		u_short	Ms_shdwsts;	/* shadow status */
		u_short Ms_track;	/* track size */
		u_short	Ms_group;	/* group size */
		u_short	Ms_cylinder;	/* cylinder size */
		u_short	Ms_xx3;		/* reserved */
		u_short	Ms_rctsize;	/* RCT table size */
		char	Ms_rbns;	/* RBNs / track */
		char	Ms_rctcpys;	/* RCT copies */
	} m_getunitsts;
	} m_un;
	int	m_msgpad[3];		/* pad msg length to 64 bytes */
					/* required by UQ bus port spec */
};

typedef struct mscp_pkt mscp_pkt_t;

/*
 * An MSCP response
 */
struct __attribute__((packed)) mscp_resp {
	uint32_t m_cmdref;		/* command reference number */

	u_short m_rsvd;
	u_short	m_seqn;			/* plus error log reference number */	

	u_char	m_endcode;		/* opcode    */
	u_char	m_flags;		/*           */
	u_short	m_status;		/* status    */
	union {
	struct {
		u_short	Ms_version;	/* MSCP version */
		u_short	Ms_cntflgs;	/* controller flags */
		u_short	Ms_timeout;	/* host timeout */
		u_char  Ms_csvrsn;  /* controller sw version */
		u_char  Ms_chvrsn;  /* controller hw version */
		mscp_id_t Ms_id;
	} m_setcntchar;
	struct {
		u_short	Ms_multiunit; /* Multiunit code */
		u_short	Ms_unitflgs;  /* unit flags */
		u_char  Ms_spindles;  /* spindle count */
		u_char  Ms_rsvd3[3];
		mscp_id_t Ms_unitid;   /* unit identifier */
		uint32_t Ms_media;    /* media type */
		uint32_t Ms_rsvd4;    /* reserved */
		uint32_t Ms_size;     /* unit size */
		uint32_t Ms_vsn;      /* Volume serial number */
	} m_online;
	struct {
		long	Ms_bytecnt;	/* byte count */
		long  	Ms_buf; 	/* buffer descriptor hi  word */
		long	Ms_xx2[2];	/* unused */
		long    Ms_lba;	    /* Bad block LBA */
		long	Ms_xx4;		/* unused */
	} m_generic;
	struct {
		uint32_t Ms_orn;
	} m_abort;
	uint8_t m_raw[48];
	} m_un;
};

typedef struct mscp_resp mscp_resp_t;

/*
 * generic packet
 */

#define	m_zzz2		m_un.m_generic.Ms_zzz2
#define	m_bytecnt	m_un.m_generic.Ms_bytecnt
#define	m_buf_l		m_un.m_generic.Ms_buf_l
#define	m_buf_h		m_un.m_generic.Ms_buf_h
#define	m_lbn_l		m_un.m_generic.Ms_lbn_l
#define	m_lbn_h		m_un.m_generic.Ms_lbn_h
#define	m_dscptr	m_un.m_generic.Ms_dscptr
#define	m_sftwds	m_un.m_generic.Ms_sftwds

/*
 * Abort / Get Command Status packet
 */

#define	m_outref	m_bytecnt

/*
 * Online / Set Unit Characteristics packet
 */

#define m_elgfll	m_lbn_l
#define m_elgflh	m_lbn_h
#define	m_copyspd	m_shdwsts

/*
 * Replace packet
 */

#define	m_rbn	m_bytecnt

/*
 * Set Controller Characteristics packet
 */

#define	m_version	m_un.m_setcntchar.Ms_version
#define	m_cntflgs	m_un.m_setcntchar.Ms_cntflgs
#define	m_hsttmo	m_un.m_setcntchar.Ms_hsttmo
#define	m_usefrac	m_un.m_setcntchar.Ms_usefrac
#define	m_time		m_un.m_setcntchar.Ms_time

/*
 * Get Unit Status end packet
 */

#define	m_multunt	m_un.m_getunitsts.Ms_multunt
#define	m_unitflgs	m_un.m_getunitsts.Ms_unitflgs
#define	m_hostid	m_un.m_getunitsts.Ms_hostid
#define	m_unitid	m_un.m_getunitsts.Ms_unitid
#define	m_mediaid	m_un.m_getunitsts.Ms_mediaid
#define	m_shdwunt	m_un.m_getunitsts.Ms_shdwunt
#define	m_shdwsts	m_un.m_getunitsts.Ms_shdwsts
#define	m_track		m_un.m_getunitsts.Ms_track
#define	m_group		m_un.m_getunitsts.Ms_group
#define	m_cylinder	m_un.m_getunitsts.Ms_cylinder
#define	m_rctsize	m_un.m_getunitsts.Ms_rctsize
#define	m_rbns		m_un.m_getunitsts.Ms_rbns
#define	m_rctcpys	m_un.m_getunitsts.Ms_rctcpys

/*
 * Online / Set Unit Characteristics end packet
 */

#define	m_uslow		m_un.m_getunitsts.Ms_track
#define	m_ushigh	m_un.m_getunitsts.Ms_group
#define	m_volser	m_sftwds[0]

/*
 * Set Controller Characteristics end packet
 */

#define	m_cnttmo	m_hsttmo
#define	m_cntcmdl	m_usefrac
#define	m_cntid		m_unitid

/*
 * MSCP Error Log packet
 *
 *	NOTE: MSCP packet must be padded to this size.
 */

typedef struct mscp_errlog {
	int	me_cmdref;		/* command reference number */
	int	me_elref;		/* error log reference number */
	short	me_unit;		/* unit number */
	short	me_seqnum;		/* sequence number */
	u_char	me_format;		/* format */
	u_char	me_flags;		/* error log message flags */
	short	me_event;		/* event code */
	quad	me_cntid;		/* controller id */
	u_char	me_cntsvr;		/* controller software version */
	u_char	me_cnthvr;		/* controller hardware version */
	short	me_multunt;		/* multi-unit code */
	quad	me_unitid;		/* unit id */
	u_char	me_unitsvr;		/* unit software version */
	u_char	me_unithvr;		/* unit hardware version */
	short	me_group;		/* group; retry + level */
	int	me_volser[2];		/* volume serial number */
	int	me_hdr[2];		/* header */
	char	me_sdistat[12];		/* SDI status information */
} mscp_errlog_t;

#define	me_busaddr	me_unitid
#define	me_sdecyl	me_group

#endif