/* 
 * (C) Copyright 1992, ..., 1998 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef DOSEMU_TIMERS_H
#define DOSEMU_TIMERS_H

#ifdef __linux__
#include <asm/param.h>		/* for HZ */
#endif
#include "extern.h"
#include "types.h"
/* moved from attremu.c */
#ifdef HAVE_GETTIMEOFDAY
#  ifdef TIME_WITH_SYS_TIME
#    include <sys/time.h>
#    include <time.h>
#  else
#    if HAVE_SYS_TIME
#      include <sys/time.h>
#    else
#      include <time.h>
#    endif
#  endif
#endif

/* A 'magical' constant used to force the result we want, in this case
 * getting 100Hz out of SIGALRM_call()
 */
#define TIMER_DIVISOR   6

extern void timer_tick(void);
extern void set_ticks(unsigned long);
extern Bit8u pit_inp(ioport_t);
extern void pit_outp(ioport_t, Bit8u);

/* The divisor used to time faster-than-1sec activities (floppy,printer,
 * IPX) - see signal.c
 */
#define PARTIALS	5

#define BIOS_TICK_ADDR		(void *)0x46c
#define TICK_OVERFLOW_ADDR	(void *)0x470

/* underlying clock rate in HZ */
#define PIT_TICK_RATE		1193182
#define PIT_TICK_RATE_k		1193.182
#define PIT_TICK_RATE_M		1.193182
#define PIT_TICK_RATE_f		0.193182

/*
 * Tick<->us conversion formulas:
 *	611/512		= 1.1933594	+150ppm
 *	19549/16384	= 1.1931763	-5ppm
 *	59659/50000	= 1.1931800	almost exact
 */

#define PIT_TIMERS      4            /* 4 timers (w/opt. at 0x44)   */

#ifndef MONOTON_MICRO_TIMING
/* This value is used by the non-monoton code in PIC. I strongly
 * disagree with it, because the value is wrong and its origin unclear.
 * But I'll keep it for compatibility  --AV */
#define PIC_TICK_RATE		1193047
#define PIC_TICK_1000   	1193         /* PIT tick rate / 1000        */
#define PIC_TICK_RATE_f		0.193047
#else /* MONOTON_MICRO_TIMING */
#ifdef i386
#   define PIT_MS2TICKS(n) ({ \
			       int res; \
			       __asm__ ("imull %%edx\n\tidivl %%ecx" \
					: "=a" (res) \
					: "a" (n), "d" (59659), "c" (50000)); \
			       res; \
			   })
#else
#   define PIT_MS2TICKS(n) ((int)(((long long)(n)*59659)/50000))
#endif
#endif /* MONOTON_MICRO_TIMING */

/* --------------------------------------------------------------------- */

#define JIFFIE_TIME		(1000000/HZ)

/* this specifies how many microseconds int 0x2f, ax=0x1680, will usleep().
 * we don't really have a "give up time slice" primitive, but something
 * like this works okay...thanks to Andrew Tridgell.
 * same for int 0x15, ax=0x1000 (TopView/DESQview).
 */
#define INT2F_IDLE_USECS	(JIFFIE_TIME*8)
#define INT15_IDLE_USECS	(JIFFIE_TIME*8)
#define INT28_IDLE_USECS	(JIFFIE_TIME/2)

/* --------------------------------------------------------------------- */

#define TIMER_TIME		ITIMER_REAL

/* --------------------------------------------------------------------- */
/*	New unified timing macros with/without Pentium rdtsc - AV 8/97	 */
/* --------------------------------------------------------------------- */
/*
 * replace 64/32 divide instructions with 64*32 multiply
 * how it works: we multiply by f = (1/x)<<32 = (1<<32)/x
 *	input = tH:tL, k
 *		tL*k ----->      edx:eax(discard)
 *		tH*k ----->  edx:eax
 *		sum	     -------
 *	output =	     edx:eax
 *
 * all operations unsigned, it really shines on K6 where mul=3 cycles!
 */
static __inline__ hitimer_t _mul64x32_(hitimer_t v, unsigned long f)
{
	hitimer_u t;

	__asm__ __volatile__ ("
		movl	%2,%%eax
		mull	%%ebx
		movl	%%edx,%%ecx
		movl	%3,%%eax
		mull	%%ebx
		addl	%%ecx,%%eax
		adcl	$0,%%edx
		"
		: "=a"(t.t.tl),"=d"(t.t.th)
		: "g"(((int *)&v)[0]),"g"(((int *)&v)[1]),\
		  "b"(f)
		: "%eax","%edx","%ecx","%ebx","memory" );
	return t.td;
}

#define LLF_US		(unsigned long long)0x100000000LL
#define LLF_TICKS	(unsigned long long)(LLF_US*PIT_TICK_RATE_M)

/* warning - 'k' must be a constant here! (but can also be a float) */
#define T64DIV(t,k)	_mul64x32_((t),(LLF_US/k))
#define TICKtoMS(t)	({unsigned long _m=T64DIV(t,PIT_TICK_RATE_k); _m;})

#define CPUtoTICK(t)	_mul64x32_((t),config.cpu_tick_spd)
/* t*1.193 = t+t*0.193 */
#define UStoTICK(t)	({ hitimer_t _l=(t); \
			_l+_mul64x32_(_l,(LLF_US*PIT_TICK_RATE_f)); })
#ifndef MONOTON_MICRO_TIMING
/* see comment above */
#define nmUStoTICK(t)	({ hitimer_t _l=(t); \
			_l+_mul64x32_(_l,(LLF_US*PIC_TICK_RATE_f)); })
#endif

#define TICKtoUS(t)	({unsigned long _m=T64DIV(t,PIT_TICK_RATE_M); _m;})

/* --------------------------------------------------------------------- */

EXTERN hitimer_t (*GETcpuTIME)(void) INIT(0);

#define CPUtoUS() \
({ \
	hitimer_u t; \
	__asm__ __volatile__ (" \
		rdtsc\n \
		movl	%%edx,%%ebx\n \
		mull	%2\n \
		movl	%%edx,%%ecx\n \
		movl	%%ebx,%%eax\n \
		mull	%2\n \
		addl	%%ecx,%%eax\n \
		adcl	$0,%%edx\n \
		" \
		: "=a"(t.t.tl),"=d"(t.t.th) \
		: "m"(config.cpu_spd) \
		: "%eax","%edx","%ecx","%ebx","memory" ); \
	t.td; \
})

#define CPUtoUS_Z() \
({ \
	hitimer_u t; \
	__asm__ __volatile__ (" \
		rdtsc\n \
		movl	%%edx,%%ebx\n \
		mull	%2\n \
		movl	%%edx,%%ecx\n \
		movl	%%ebx,%%eax\n \
		mull	%2\n \
		addl	%%ecx,%%eax\n \
		adcl	$0,%%edx\n \
		subl	%3,%%eax\n \
		sbbl	%4,%%edx\n \
		" \
		: "=a"(t.t.tl),"=d"(t.t.th) \
		: "m"(config.cpu_spd), \
		  "m"(ZeroTimeBase.t.tl),"m"(ZeroTimeBase.t.th) \
		: "%eax","%edx","%ecx","%ebx","memory" ); \
	t.td; \
})

/* 1 us granularity */
extern hitimer_t GETusTIME(int sc);
extern hitimer_t GETusSYSTIME(void);

/* 838 ns granularity */
extern hitimer_t GETtickTIME(int sc);

int stop_cputime (void);
int restart_cputime (void);
extern int cpu_time_stop;	/* for dosdebug */
int getmhz(void);		/* for init/config.c */
int bogospeed(unsigned long *spus, unsigned long *sptick);

/* --------------------------------------------------------------------- */

extern Bit8u pit_control_inp(ioport_t);
extern void pit_control_outp(ioport_t port, Bit8u val);
extern void initialize_timers(void);
extern void get_time_init(void);

#endif /* DOSEMU_TIMERS_H */
