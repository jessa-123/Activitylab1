/* 
 * (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef DOSEMU_CPUEMU_H
#define DOSEMU_CPUEMU_H

#include <signal.h>
#include "bitops.h"

/*
 * Size of io_bitmap in longwords: 32 is ports 0-0x3ff.
 */
#define IO_BITMAP_SIZE	32

extern unsigned long	io_bitmap[IO_BITMAP_SIZE+1];
extern void e_priv_iopl(int);
#define test_ioperm(a)	((a)>0x3ff? 0:test_bit((a),io_bitmap))

/* ------------------------------------------------------------------------
 * if this is defined, cpuemu will only start when explicitly activated
 * by int0xe6. Technically: if we want to trace DOS boot we undefine this
 * and config.cpuemu will be set at 3 from the start - else it is set at 1
 */
#if 1
#define DONT_START_EMU
#endif

/* define this to skip emulation through video BIOS. This speeds up a lot
 * the cpuemu and avoids possible time-dependent code in the VBIOS. It has
 * a drawback: when we jump into REAL vm86 in the VBIOS, we never know
 * where we will exit back to the cpuemu. Oh, well, speed is what matters...
 */
#if 1
#define	SKIP_EMU_VBIOS
#endif

/* if defined, trace instructions (with debug_level('e')>3) only in protected
 * mode code. This is useful to skip timer interrupts and/or better
 * follow the instruction flow */
#if 0
#define SKIP_VM86_TRACE
#endif

/* If you set this to 1, some I/O instructions will be compiled in,
 * otherwise all I/O will be interpreted.
 * Because of fault overhead, only instructions which access an
 * untrapped port will be allowed to compile. This is not 100% safe
 * since DX can dynamically change.
 */
#if 1
#define CPUEMU_DIRECT_IO
#endif

/* ----------------------------------------------------------------------- */

/* Cpuemu status register - pack as much info as possible here, so to
 * use a single test to check if we have to go further or not */
#define CeS_SIGPEND	0x01	/* signal pending mask */
#define CeS_SIGACT	0x02	/* signal active mask */
#define CeS_RPIC	0x04	/* pic asks for interruption */
#define CeS_STI		0x08	/* IF active was popped */
#define CeS_LOCK	0x800	/* cycle lock (pop ss; pop sp et sim.) */
#define CeS_TRAP	0x1000	/* INT01 Sstep active */
#define CeS_DRTRAP	0x2000	/* Debug Registers active */

extern int IsV86Emu;
extern int IsDpmiEmu;

extern volatile int CEmuStat;
extern int InCompiledCode;

void enter_cpu_emu(void);
void leave_cpu_emu(void);
void avltr_destroy(void);

#define FLUSH_TREE	if (config.cpuemu>1) avltr_destroy()

int s_munprotect(caddr_t addr);
int s_mprotect(caddr_t addr);

#define E_MUNPROT_STACK(s)	(config.cpuemu>1? s_munprotect((caddr_t)(s)):0)
#define E_MPROT_STACK(s)	(config.cpuemu>1? s_mprotect((caddr_t)(s)):0)

/* called from dpmi.c */
void emu_mhp_SetTypebyte (unsigned short selector, int typebyte);
unsigned short emu_do_LAR (unsigned short selector);

/* called from mfs.c */
int e_dos_read(int fd, char *data, int cnt);

/* called from cpu.c */
void init_emu_cpu (void);

/* called/used from dpmi.c */
int e_dpmi(struct sigcontext_struct *scp);
void e_dpmi_b0x(int op,struct sigcontext_struct *scp);
extern int emu_dpmi_retcode;
extern int in_dpmi_emu;

#endif	/*DOSEMU_CPUEMU_H*/
