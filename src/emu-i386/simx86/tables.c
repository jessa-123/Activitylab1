/***************************************************************************
 * 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2001 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 *
 *
 *  SIMX86 a Intel 80x86 cpu emulator
 *  Copyright (C) 1997,2001 Alberto Vignani, FIAT Research Center
 *				a.vignani@crf.it
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Additional copyright notes:
 *
 * 1. The kernel-level vm86 handling was taken out of the Linux kernel
 *  (linux/arch/i386/kernel/vm86.c). This code originaly was written by
 *  Linus Torvalds with later enhancements by Lutz Molgedey and Hans Lermen.
 *
 ***************************************************************************/

#include "emu86.h"
#include "codegen.h"

/////////////////////////////////////////////////////////////////////////////

unsigned char byrev[256] = {
	0x00,0x80,0x40,0xc0,0x20,0xa0,0x60,0xe0,
	0x10,0x90,0x50,0xd0,0x30,0xb0,0x70,0xf0,
	0x08,0x88,0x48,0xc8,0x28,0xa8,0x68,0xe8,
	0x18,0x98,0x58,0xd8,0x38,0xb8,0x78,0xf8,
	0x04,0x84,0x44,0xc4,0x24,0xa4,0x64,0xe4,
	0x14,0x94,0x54,0xd4,0x34,0xb4,0x74,0xf4,
	0x0c,0x8c,0x4c,0xcc,0x2c,0xac,0x6c,0xec,
	0x1c,0x9c,0x5c,0xdc,0x3c,0xbc,0x7c,0xfc,
	0x02,0x82,0x42,0xc2,0x22,0xa2,0x62,0xe2,
	0x12,0x92,0x52,0xd2,0x32,0xb2,0x72,0xf2,
	0x0a,0x8a,0x4a,0xca,0x2a,0xaa,0x6a,0xea,
	0x1a,0x9a,0x5a,0xda,0x3a,0xba,0x7a,0xfa,
	0x06,0x86,0x46,0xc6,0x26,0xa6,0x66,0xe6,
	0x16,0x96,0x56,0xd6,0x36,0xb6,0x76,0xf6,
	0x0e,0x8e,0x4e,0xce,0x2e,0xae,0x6e,0xee,
	0x1e,0x9e,0x5e,0xde,0x3e,0xbe,0x7e,0xfe,
	0x01,0x81,0x41,0xc1,0x21,0xa1,0x61,0xe1,
	0x11,0x91,0x51,0xd1,0x31,0xb1,0x71,0xf1,
	0x09,0x89,0x49,0xc9,0x29,0xa9,0x69,0xe9,
	0x19,0x99,0x59,0xd9,0x39,0xb9,0x79,0xf9,
	0x05,0x85,0x45,0xc5,0x25,0xa5,0x65,0xe5,
	0x15,0x95,0x55,0xd5,0x35,0xb5,0x75,0xf5,
	0x0d,0x8d,0x4d,0xcd,0x2d,0xad,0x6d,0xed,
	0x1d,0x9d,0x5d,0xdd,0x3d,0xbd,0x7d,0xfd,
	0x03,0x83,0x43,0xc3,0x23,0xa3,0x63,0xe3,
	0x13,0x93,0x53,0xd3,0x33,0xb3,0x73,0xf3,
	0x0b,0x8b,0x4b,0xcb,0x2b,0xab,0x6b,0xeb,
	0x1b,0x9b,0x5b,0xdb,0x3b,0xbb,0x7b,0xfb,
	0x07,0x87,0x47,0xc7,0x27,0xa7,0x67,0xe7,
	0x17,0x97,0x57,0xd7,0x37,0xb7,0x77,0xf7,
	0x0f,0x8f,0x4f,0xcf,0x2f,0xaf,0x6f,0xef,
	0x1f,0x9f,0x5f,0xdf,0x3f,0xbf,0x7f,0xff
};

/////////////////////////////////////////////////////////////////////////////

/* table of opcode properties
 *
 *	bit 7	1=jmp instr
 *	bit 6	1=special subtable
 *	bit 5	1=op modifies flags
 *	bit 4	1=op can write into memory (excl.stack)
 *	bit 3	1=has mod/r_m
 *	bit 2	1=prefix
 *	bit 1	1=force DPMI_TRACE registers display(?)
 *	bit 0	0=can be compiled	1=always interpreted
 */

char InterOps[256] =
{
	0x38,	// ADDbfrm		0x00
	0x38,	// ADDwfrm		0x01
	0x28,	// ADDbtrm		0x02
	0x28,	// ADDwtrm		0x03
	0x28,	// ADDbia		0x04
	0x28,	// ADDwia		0x05
	0x00,	// PUSHes		0x06
	0x02,	// POPes		0x07
	0x38,	// ORbfrm		0x08
	0x38,	// ORwfrm		0x09
	0x28,	// ORbtrm		0x0a
	0x28,	// ORwtrm		0x0b
	0x28,	// ORbi			0x0c
	0x28,	// ORwi			0x0d
	0x00,	// PUSHcs		0x0e
	0x72,	// TwoByteESC		0x0f
	0x38,	// ADCbfrm		0x10
	0x38,	// ADCwfrm		0x11
	0x28,	// ADCbtrm		0x12
	0x28,	// ADCwtrm		0x13
	0x28,	// ADCbi		0x14
	0x28,	// ADCwi		0x15
	0x00,	// PUSHss		0x16
	0x02,	// POPss		0x17
	0x38,	// SBBbfrm		0x18
	0x38,	// SBBwfrm		0x19
	0x28,	// SBBbtrm		0x1a
	0x28,	// SBBwtrm		0x1b
	0x28,	// SBBbi		0x1c
	0x28,	// SBBwi		0x1d
	0x00,	// PUSHds		0x1e
	0x02,	// POPds		0x1f
	0x38,	// ANDbfrm		0x20
	0x38,	// ANDwfrm		0x21
	0x28,	// ANDbtrm		0x22
	0x28,	// ANDwtrm		0x23
	0x28,	// ANDbi		0x24
	0x28,	// ANDwi		0x25
	0x04,	// SEGes		0x26
	0x20,	// DAA			0x27
	0x38,	// SUBbfrm		0x28
	0x38,	// SUBwfrm		0x29
	0x28,	// SUBbtrm		0x2a
	0x28,	// SUBwtrm		0x2b
	0x28,	// SUBbi		0x2c
	0x28,	// SUBwi		0x2d
	0x04,	// SEGcs		0x2e
	0x20,	// DAS			0x2f
	0x38,	// XORbfrm		0x30
	0x38,	// XORwfrm		0x31
	0x28,	// XORbtrm		0x32
	0x28,	// XORwtrm		0x33
	0x28,	// XORbi		0x34
	0x28,	// XORwi		0x35
	0x04,	// SEGss		0x36
	0x20,	// AAA			0x37
	0x28,	// CMPbfrm		0x38
	0x28,	// CMPwfrm		0x39
	0x28,	// CMPbtrm		0x3a
	0x28,	// CMPwtrm		0x3b
	0x28,	// CMPbi		0x3c
	0x28,	// CMPwi		0x3d
	0x04,	// SEGds		0x3e
	0x20,	// AAS			0x3f
	0x20,	// INCax		0x40
	0x20,	// INCcx		0x41
	0x20,	// INCdx		0x42
	0x20,	// INCbx		0x43
	0x20,	// INCsp		0x44
	0x20,	// INCbp		0x45
	0x20,	// INCsi		0x46
	0x20,	// INCdi		0x47
	0x20,	// DECax		0x48
	0x20,	// DECcx		0x49
	0x20,	// DECdx		0x4a
	0x20,	// DECbx		0x4b
	0x20,	// DECsp		0x4c
	0x20,	// DECbp		0x4d
	0x20,	// DECsi		0x4e
	0x20,	// DECdi		0x4f
	0x00,	// PUSHax		0x50
	0x00,	// PUSHcx		0x51
	0x00,	// PUSHdx		0x52
	0x00,	// PUSHbx		0x53
	0x00,	// PUSHsp		0x54
	0x00,	// PUSHbp		0x55
	0x00,	// PUSHsi		0x56
	0x00,	// PUSHdi		0x57
	0x00,	// POPax		0x58
	0x00,	// POPcx		0x59
	0x00,	// POPdx		0x5a
	0x00,	// POPbx		0x5b
	0x00,	// POPsp		0x5c
	0x00,	// POPbp		0x5d
	0x00,	// POPsi		0x5e
	0x00,	// POPdi		0x5f
	0x00,	// PUSHA		0x60
	0x00,	// POPA			0x61
	0x81,	// BOUND		0x62
	0x81,	// ARPL			0x63
	0x04,	// SEGfs		0x64
	0x04,	// SEGgs		0x65
	0x04,	// OPERoverride		0x66
	0x04,	// ADDRoverride		0x67
	0x00,	// PUSHwi		0x68
	0x28,	// IMULwrm		0x69 
	0x00,	// PUSHbi		0x6a
	0x28,	// IMULbrm		0x6b
	0x11,	// INSb			0x6c
	0x11,	// INSw			0x6d
	0x01,	// OUTSb		0x6e
	0x01,	// OUTSw		0x6f
	0x82,	// JO			0x70
	0x82,	// JNO			0x71
	0x82,	// JB_JNAE		0x72
	0x82,	// JNB_JAE		0x73
	0x82,	// JE_JZ		0x74
	0x82,	// JNE_JNZ		0x75
	0x82,	// JBE_JNA		0x76
	0x82,	// JNBE_JA		0x77
	0x82,	// JS			0x78
	0x82,	// JNS			0x79
	0x82,	// JP_JPE		0x7a
	0x82,	// JNP_JPO		0x7b
	0x82,	// JL_JNGE		0x7c
	0x82,	// JNL_JGE		0x7d
	0x82,	// JLE_JNG		0x7e
	0x82,	// JNLE_JG		0x7f
	0x70,	// IMMEDbrm		0x80
	0x70,	// IMMEDwrm		0x81
	0x70,	// IMMEDbrm2		0x82
	0x70,	// IMMEDisrm		0x83
	0x28,	// TESTbrm		0x84
	0x28,	// TESTwrm		0x85
	0x18,	// XCHGbrm		0x86
	0x18,	// XCHGwrm		0x87
	0x18,	// MOVbfrm		0x88
	0x18,	// MOVwfrm		0x89
	0x08,	// MOVbtrm		0x8a
	0x08,	// MOVwtrm		0x8b
	0x08,	// MOVsrtrm		0x8c
	0x08,	// LEA			0x8d
	0x1a,	// MOVsrfrm		0x8e
	0x18,	// POPrm		0x8f
	0x00,	// NOP			0x90
	0x00,	// XCHGcx		0x91
	0x00,	// XCHGdx		0x92
	0x00,	// XCHGbx		0x93
	0x00,	// XCHGsp		0x94
	0x00,	// XCHGbp		0x95
	0x00,	// XCHGsi		0x96
	0x00,	// XCHGdi		0x97
	0x00,	// CBW			0x98
	0x00,	// CWD			0x99
	0x01,	// CALLl		0x9a
	0x01,	// WAIT			0x9b
	0x00,	// PUSHF		0x9c
	0x21,	// POPF			0x9d
	0x20,	// SAHF			0x9e
	0x00,	// LAHF			0x9f
	0x00,	// MOVmal		0xa0
	0x00,	// MOVmax		0xa1
	0x10,	// MOValm		0xa2
	0x10,	// MOVaxm		0xa3
	0x10,	// MOVSb		0xa4
	0x10,	// MOVSw		0xa5
	0x20,	// CMPSb		0xa6
	0x20,	// CMPSw		0xa7
	0x20,	// TESTbi		0xa8
	0x20,	// TESTwi		0xa9
	0x10,	// STOSb		0xaa
	0x10,	// STOSw		0xab
	0x00,	// LODSb		0xac
	0x00,	// LODSw		0xad
	0x20,	// SCASb		0xae
	0x20,	// SCASw		0xaf
	0x00,	// MOVial		0xb0
	0x00,	// MOVicl		0xb1
	0x00,	// MOVidl		0xb2
	0x00,	// MOVibl		0xb3
	0x00,	// MOViah		0xb4
	0x00,	// MOVich		0xb5
	0x00,	// MOVidh		0xb6
	0x00,	// MOVibh		0xb7
	0x00,	// MOViax		0xb8
	0x00,	// MOVicx		0xb9
	0x00,	// MOVidx		0xba
	0x00,	// MOVibx		0xbb
	0x00,	// MOVisp		0xbc
	0x00,	// MOVibp		0xbd
	0x00,	// MOVisi		0xbe
	0x00,	// MOVidi		0xbf
	0x38,	// SHIFTbi		0xc0
	0x38,	// SHIFTwi		0xc1 
	0x01,	// RETisp		0xc2
	0x01,	// RET			0xc3
	0x02,	// LES			0xc4
	0x02,	// LDS			0xc5
	0x18,	// MOVbirm		0xc6
	0x18,	// MOVwirm		0xc7
	0x01,	// ENTER		0xc8
	0x00,	// LEAVE		0xc9 
	0x01,	// RETlisp		0xca
	0x01,	// RETl			0xcb
	0x01,	// INT3			0xcc
	0x01,	// INT			0xcd
	0x01,	// INTO			0xce
	0x21,	// IRET			0xcf
	0x38,	// SHIFTb		0xd0
	0x38,	// SHIFTw		0xd1
	0x38,	// SHIFTbv		0xd2
	0x38,	// SHIFTwv		0xd3
	0x20,	// AAM			0xd4
	0x20,	// AAD			0xd5
	0x81,	// RESERVED1		0xd6
	0x00,	// XLAT			0xd7
	0x72,	// ESC0			0xd8
	0x72,	// ESC1			0xd9
	0x72,	// ESC2			0xda
	0x72,	// ESC3			0xdb
	0x72,	// ESC4			0xdc
	0x72,	// ESC5			0xdd
	0x72,	// ESC6			0xde
	0x72,	// ESC7			0xdf
	0x83,	// LOOPNZ_LOOPNE	0xe0
	0x83,	// LOOPZ_LOOPE		0xe1
	0x83,	// LOOP			0xe2
	0x02,	// JCXZ			0xe3
	0x01,	// INb			0xe4
	0x01,	// INw			0xe5
	0x01,	// OUTb			0xe6
	0x01,	// OUTw			0xe7
	0x80,	// CALLd		0xe8
	0x82,	// JMPd			0xe9
	0x81,	// JMPld		0xea
	0x82,	// JMPsid		0xeb
#ifdef CPUEMU_DIRECT_IO
	0x00,	// INvb			0xec
	0x00,	// INvw			0xed
	0x00,	// OUTvb		0xee
	0x00,	// OUTvw		0xef
#else
	0x01,	// INvb			0xec
	0x01,	// INvw			0xed
	0x01,	// OUTvb		0xee
	0x01,	// OUTvw		0xef
#endif
	0x85,	// LOCK			0xf0
	0x81,	// BARTS_OP		0xf1
	0x04,	// REPNE		0xf2
	0x04,	// REP			0xf3
	0x01,	// HLT			0xf4
	0x20,	// CMC			0xf5
	0x60,	// GRP1brm 		0xf6
	0x60,	// GRP1wrm 		0xf7
	0x20,	// CLC			0xf8
	0x20,	// STC			0xf9
	0x21,	// CLI			0xfa
	0x21,	// STI			0xfb
	0x20,	// CLD			0xfc
	0x20,	// STD			0xfd
	0x70,	// GRP2brm		0xfe
	0x72,	// GRP2wrm		0xff
};


/////////////////////////////////////////////////////////////////////////////


int GendBytesPerOp[128] =
{
/*  0*/	 1,
/*  1*/  6, 9,15,18,19,19,50, 0, 0,
/* 10*/  4, 4, 8, 7, 5,10, 4, 8,10,18, 0, 6, 3, 5, 0, 0, 0, 0, 0, 0,
/* 30*/  7, 7, 7, 7, 7, 7, 7, 7, 6, 6,
/* 40*/  6, 7, 7, 9,16,16,12,12, 3, 5,
/* 50*/ 17,17,28,28, 8, 8, 5, 8, 8, 8,
/* 60*/  8,10,22, 0, 6,10, 0, 5, 6, 9,
/* 70*/  8, 9, 8, 0, 0, 0, 0, 0, 0, 0,
/* 80*/	24,25,23,76,76, 6,14,51, 3, 6,14, 3,24,
/* 93*/  0, 0, 0, 0, 0, 0, 0,
/*100*/ 24,16,18,11,16, 7, 7,48, 7, 0, 8, 0,32,44,33,26,
/*116*/  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


char RmIsReg[256] =
{
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,	// bit 1 = same reg
	3,1,1,1,1,1,1,1,1,3,1,1,1,1,1,1,	// 11000000 11001001
	1,1,3,1,1,1,1,1,1,1,1,3,1,1,1,1,	// 11010010 11011011
	1,1,1,1,3,1,1,1,1,1,1,1,1,3,1,1,	// 11100100 11101101
	1,1,1,1,1,1,3,1,1,1,1,1,1,1,1,3		// 11110110 11111111
};

char OpIsPush[256] =
{
	0,0,0,0,0,0, 9,0,0,0,0,0,0,0,10,0,	// 06 0e
	0,0,0,0,0,0,11,0,0,0,0,0,0,0,12,0,	// 16 1e
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,2,3,4,5,6,7,8,0,0,0,0,0,0,0,0,	// 50..57
	0,0,0,0,0,0,13,0,0,0,0,0,0,0,0,0,	// 66
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

/////////////////////////////////////////////////////////////////////////////

