/* 
 * (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* 
 * SIDOC_BEGIN_MODULE
 *
 * Description: New port handling code for DOSEMU
 * 
 * Maintainers: Alberto Vignani (vignani@mail.tin.it)
 *
 * REMARK
 * This is the code that allows and disallows port access within DOSEMU.
 * The BOCHS port IO code was actually very cleverly done.  So the idea
 * was stolen from there.
 *
 * This port I/O code (previously in portss.c, from Scott Bucholz) is based on
 * a table access instead of a switch statement. This method is much more
 * clean and easy to maintain, while not slower than a switch.
 *
 * Remains of the old code are emerging here and there, they will
 * hopefully be moved back to where they belong, mainly video code.
 * /REMARK
 *
 * SIDOC_END_MODULE
 *
 */

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/io.h>

#include "emu.h"
#include "port.h"
#include "timers.h"
#include "video.h"
#include "vgaemu.h"	/* for video retrace */
#include "hgc.h"
#include "bios.h"
#include "vc.h"
#include "shared.h"
#include "serial.h"
#include "bitops.h"
#ifdef X86_EMULATOR
#include "cpu-emu.h"
#include "bitops.h"
#endif

_port_handler port_handler[EMU_MAX_IO_DEVICES];
unsigned char port_handle_table[0x10000];
unsigned char port_andmask[0x400];
unsigned char port_ormask[0x400];

static unsigned char port_handles;	/* number of io_handler's */

static const char *irq_handler_name[EMU_MAX_IRQS];

#define SET_HANDLE(p,h)		port_handle_table[(Bit16u)(p)]=(h)
#define EMU_HANDLER(port)	port_handler[port_handle_table[(Bit16u)(port)]]
enum{TYPE_INB, TYPE_OUTB, TYPE_INW, TYPE_OUTW, TYPE_IND, TYPE_OUTD, TYPE_EXIT};

/* any user or system device which creates a new handle can't be later
 * remapped by extra_port_init()
 */
void SET_HANDLE_COND(int p, int h)
{
  if (port_handle_table[(p)] < STD_HANDLES)
	port_handle_table[(p)]=(h);
  else
	i_printf("PORT: %#x can't be mapped to handle %#x(%#x)\n",
	  p,h,port_handle_table[(p)]);
}

/* ---------------------------------------------------------------------- */
/* PORT TRACING								  */

#if 0
static long nyb2bin[16] =
{
	0x30303030, 0x31303030, 0x30313030, 0x31313030,
	0x30303130, 0x31303130, 0x30313130, 0x31313130,
	0x30303031, 0x31303031, 0x30313031, 0x31313031,
	0x30303131, 0x31303131, 0x30313131, 0x31313131
};

static char *
 p2bin(unsigned char c)
{
	static char s[16] = "   [00000000]";

	((long *) s)[1] = nyb2bin[(c >> 4) & 15];
	((long *) s)[2] = nyb2bin[c & 15];

	return s + 3;
}
#endif

#define PORTLOG_MAXBITS		16
#define PORTLOG_MASK		(~(-1 << PORTLOG_MAXBITS))
#define SIZE_PORTLOGMAP		(1 << (PORTLOG_MAXBITS -3))
static unsigned long *portlog_map = 0;

void init_port_traceing(void);

void register_port_traceing(ioport_t firstport, ioport_t lastport)
{
  firstport &= PORTLOG_MASK;
  lastport &= PORTLOG_MASK;
  if (lastport < firstport) return;
  init_port_traceing();
  for (; firstport <= lastport; firstport++) {
    set_bit(firstport, portlog_map);
  }
}

void clear_port_traceing(void)
{
  if (!portlog_map) portlog_map = malloc(SIZE_PORTLOGMAP);
  memset(portlog_map, 0, SIZE_PORTLOGMAP);
}

void init_port_traceing(void)
{
  if (portlog_map) return;
  clear_port_traceing();
  register_port_traceing(0x0, 0xffff);
}

#define TT_printf(p,f,v,m) ({ \
  if (debug_level('T') && test_bit(p & PORTLOG_MASK, portlog_map)) { \
    log_printf(1, "%hx %c %x\n", p, f, v & m); \
  } \
})

static Bit8u log_port_read(ioport_t port, Bit8u r)
{
  TT_printf(port, '>', r, 0xff);
  return r;
}

static Bit16u log_port_read_w(ioport_t port, Bit16u r)
{
  TT_printf(port, '}', r, 0xffff);
  return r;
}

static Bit32u log_port_read_d(ioport_t port, Bit32u r)
{
  TT_printf(port, ']', r, 0xffffffff);
  return r;
}

static void log_port_write(ioport_t port, Bit8u w)
{
  TT_printf(port, '<', w, 0xff);
}

static void log_port_write_w(ioport_t port, Bit16u w)
{
  TT_printf(port, '{', w, 0xffff);
}

static void log_port_write_d(ioport_t port, Bit32u w)
{
  TT_printf(port, '[', w, 0xffffffff);
}

#define LOG_PORT_READ(port, r) (debug_level('T') ? log_port_read(port, r) : r)
#define LOG_PORT_READ_W(port, r) (debug_level('T') ? log_port_read_w(port, r) : r)
#define LOG_PORT_READ_D(port, r) (debug_level('T') ? log_port_read_d(port, r) : r)

#define LOG_PORT_WRITE(port, w) do{ if (debug_level('T')) log_port_write(port, w); }while(0)
#define LOG_PORT_WRITE_W(port, w) do{ if (debug_level('T')) log_port_write_w(port, w); }while(0)
#define LOG_PORT_WRITE_D(port, w) do{ if (debug_level('T')) log_port_write_d(port, w); }while(0)

/* ---------------------------------------------------------------------- */
/* SIDOC_BEGIN_REMARK
 *
 * The following port_{in|out}{bwd} functions are the main entry points to
 * the port code. They look into the port_handle_table and call the
 * appropriate code, usually the std_port_ functions, but each device is
 * free to register its own functions which in turn will call std_port or
 * directly access I/O (like video code does), or emulate it - AV
 *
 * SIDOC_END_REMARK
 */
/* 
 * SIDOC_BEGIN_FUNCTION port_inb(ioport_t port)
 *
 * Handles/simulates an inb() port IO read
 *
 * SIDOC_END_FUNCTION
 */
Bit8u port_inb(ioport_t port)
{
	Bit8u res;
	res = EMU_HANDLER(port).read_portb(port);
	return LOG_PORT_READ(port, res);
}

/* 
 * SIDOC_BEGIN_FUNCTION port_outb(ioport_t port, Bit8u byte)
 *
 * Handles/simulates an outb() port IO write
 *
 * SIDOC_END_FUNCTION
 */
void port_outb(ioport_t port, Bit8u byte)
{
	LOG_PORT_WRITE(port, byte);
	EMU_HANDLER(port).write_portb(port,byte);
}

/* 
 * SIDOC_BEGIN_FUNCTION port_inw(ioport_t port)
 *
 * Handles/simulates an inw() port IO read.  Usually this invokes
 * port_inb() twice, but it may be necessary to do full word i/o for
 * some video boards.
 *
 * SIDOC_END_FUNCTION
 */
Bit16u port_inw(ioport_t port)
{
	Bit16u res;

	if (EMU_HANDLER(port).read_portw != NULL) {
		res = EMU_HANDLER(port).read_portw(port);
		return LOG_PORT_READ_W(port, res);
	}
	else {
		res = (Bit16u) port_inb(port) | (((Bit16u) port_inb(port + 1)) << 8);
	}
	return res;
}

/* 
 * SIDOC_BEGIN_FUNCTION port_outw(ioport_t port, Bit16u word)
 *
 * Handles/simulates an outw() port IO write
 *
 * SIDOC_END_FUNCTION
 */
void port_outw(ioport_t port, Bit16u word)
{
	if (EMU_HANDLER(port).write_portw != NULL) {
		LOG_PORT_WRITE_W(port, word);
		EMU_HANDLER(port).write_portw(port, word);
	}
	else {
		port_outb(port, word & 0xff);
		port_outb(port+1, (word >> 8) & 0xff);
	}
}

/* 
 * SIDOC_BEGIN_FUNCTION port_ind(ioport_t port)
 * SIDOC_BEGIN_FUNCTION port_outd(ioport_t port, Bit32u dword)
 *
 * Handles/simulates an ind()/outd() port IO read/write.
 *
 * SIDOC_END_FUNCTION
 */
Bit32u port_ind(ioport_t port)
{
	Bit32u res;

	if (EMU_HANDLER(port).read_portd != NULL) {
		res = EMU_HANDLER(port).read_portd(port);
	}
	else {
		res = (Bit32u) port_inw(port) | (((Bit32u) port_inw(port + 2)) << 16);
	}
	return LOG_PORT_READ_D(port, res);
}

void port_outd(ioport_t port, Bit32u dword)
{
	LOG_PORT_WRITE_D(port, dword);
	if (EMU_HANDLER(port).write_portd != NULL) {
		EMU_HANDLER(port).write_portd(port, dword);
	}
	else {
		port_outw(port, dword & 0xffff);
		port_outw(port+2, (dword >> 16) & 0xffff);
	}
}


/* ---------------------------------------------------------------------- */
/* default port I/O access
 */

struct portreq 
{
        ioport_t port;
        int type;
        unsigned long word;
};

static int port_fd_out[2];
static int port_fd_in[2];

Bit8u std_port_inb(ioport_t port)
{
        struct portreq pr;
        pr.port = port;
        pr.type = TYPE_INB;
	write(port_fd_out[1], &pr, sizeof(pr));
	read(port_fd_in[0], &pr, sizeof(pr));
	return pr.word;
}

void std_port_outb(ioport_t port, Bit8u byte)
{
        struct portreq pr;
        pr.word = byte;
        pr.port = port;
        pr.type = TYPE_OUTB;
	write(port_fd_out[1], &pr, sizeof(pr));
}

Bit16u std_port_inw(ioport_t port)
{
        struct portreq pr;
        pr.port = port;
        pr.type = TYPE_INW;
	write(port_fd_out[1], &pr, sizeof(pr));
	read(port_fd_in[0], &pr, sizeof(pr));
	return pr.word;
}

void std_port_outw(ioport_t port, Bit16u word)
{
        struct portreq pr;
        pr.word = word;
        pr.port = port;
        pr.type = TYPE_OUTW;
	write(port_fd_out[1], &pr, sizeof(pr));
}

Bit32u std_port_ind(ioport_t port)
{
        struct portreq pr;
        pr.port = port;
        pr.type = TYPE_IND;
	write(port_fd_out[1], &pr, sizeof(pr));
	read(port_fd_in[0], &pr, sizeof(pr));
	return pr.word;
}

void std_port_outd(ioport_t port, Bit32u dword)
{
        struct portreq pr;
        pr.word = dword;
        pr.port = port;
        pr.type = TYPE_OUTD;
	write(port_fd_out[1], &pr, sizeof(pr));
}

/* ---------------------------------------------------------------------- */
/* the following functions are all static!				  */

static void pna_emsg(ioport_t port, char ch, char *s)
{
	i_printf("PORT%c: %x not available for %s\n", ch, port, s);
}

static Bit8u port_not_avail_inb(ioport_t port)
{
/* it is a fact of (hardware) life that unused locations return all
   (or almost all) the bits at 1; some software can try to detect a
   card basing on this fact and fail if it reads 0x00 - AV */
	if (debug_level('i')) pna_emsg(port,'b',"read");
	return 0xff;
}

static void port_not_avail_outb(ioport_t port, Bit8u byte)
{
	if (debug_level('i')) pna_emsg(port,'b',"write");
}

static Bit16u port_not_avail_inw(ioport_t port)
{
	if (debug_level('i')) pna_emsg(port,'w',"read");
	return 0xffff;
}

static void port_not_avail_outw(ioport_t port, Bit16u value)
{
	if (debug_level('i')) pna_emsg(port,'w',"write");
}

static Bit32u port_not_avail_ind(ioport_t port)
{
	if (debug_level('i')) pna_emsg(port,'d',"read");
	return 0xffffffff;
}

static void port_not_avail_outd(ioport_t port, Bit32u value)
{
	if (debug_level('i')) pna_emsg(port,'d',"write");
}


/* ---------------------------------------------------------------------- */
/* SIDOC_BEGIN_REMARK
 *
 * optimized versions for rep - basically we avoid changing privileges
 * and iopl on and off lots of times. We are safe letting iopl=3 here
 * since we don't exit from this code until finished.
 * This code is shared between VM86 and DPMI.
 *
 * SIDOC_END_REMARK
 */

int port_rep_inb(ioport_t port, Bit8u *base, int df, Bit32u count)
{
	register int incr = df? -1: 1;
	Bit8u *dest = base;
	int count_ = count;

	if (count==0) return 0;
	i_printf("Doing REP insb(%#x) %d bytes at %p, DF %d\n", port,
		count, base, df);
	if (EMU_HANDLER(port).read_portb == std_port_inb) {
	    while (count--) {
	      *dest = std_port_inb(port);
	      dest += incr;
	    }
	}
	else {
	  while (count--) {
	    *dest = EMU_HANDLER(port).read_portb(port);
	    dest += incr;
	  }
	}
	if (debug_level('T')) {
		dest = base;
		while (count_--) {
			LOG_PORT_READ(port, *dest);
			dest += incr;
		}
	}
	return dest-base;
}

int port_rep_outb(ioport_t port, Bit8u *base, int df, Bit32u count)
{
	register int incr = df? -1: 1;
	Bit8u *dest = base;
	int count_ = count;

	if (count==0) return 0;
	i_printf("Doing REP outsb(%#x) %d bytes at %p, DF %d\n", port,
		count, base, df);
	if (EMU_HANDLER(port).write_portb == std_port_outb) {
	    while (count--) {
	      std_port_outb(port, *dest);
	      dest += incr;
	    }
	}
	else {
	  while (count--) {
	    EMU_HANDLER(port).write_portb(port, *dest);
	    dest += incr;
	  }
	}
	if (debug_level('T')) {
		dest = base;
		while (count_--) {
			LOG_PORT_WRITE(port, *dest);
			dest += incr;
		}
	}
	return dest-base;
}

int port_rep_inw(ioport_t port, Bit16u *base, int df, Bit32u count)
{
	register int incr = df? -1: 1;
	Bit16u *dest = base;
	int count_ = count;

	if (count==0) return 0;
	i_printf("Doing REP insw(%#x) %d words at %p, DF %d\n", port,
		count, base, df);
	if (EMU_HANDLER(port).read_portw == std_port_inw) {
	    while (count--) {
	      *dest = std_port_inw(port);
	      dest += incr;
	    }
	}
	else if (EMU_HANDLER(port).read_portw == NULL) {
	  Bit16u res;
	  while (count--) {
	    res = EMU_HANDLER(port).read_portb(port);
	    *dest = ((Bit16u)EMU_HANDLER(port).read_portb(port+1) <<8) | res;
	    dest += incr;
	  }
	}
	else {
	  while (count--) {
	    *dest = EMU_HANDLER(port).read_portw(port);
	    dest += incr;
	  }
	}
	if (debug_level('T')) {
		dest = base;
		while (count_--) {
			LOG_PORT_READ_W(port, *dest);
			dest += incr;
		}
	}
	return (Bit8u *)dest-(Bit8u *)base;
}

int port_rep_outw(ioport_t port, Bit16u *base, int df, Bit32u count)
{
	register int incr = df? -1: 1;
	Bit16u *dest = base;
	int count_ = count;

	if (count==0) return 0;
	i_printf("Doing REP outsw(%#x) %d words at %p, DF %d\n", port,
		count, base, df);
	if (EMU_HANDLER(port).write_portw == std_port_outw) {
	    while (count--) {
	      std_port_outw(port, *dest);
	      dest += incr;
	    }
	}
	else if (EMU_HANDLER(port).write_portw == NULL) {
	  Bit16u res;
	  while (count--) {
	    res = *dest, dest += incr;
	    EMU_HANDLER(port).write_portb(port, res);
	    EMU_HANDLER(port).write_portb(port+1, res>>8);
	  }
	}
	else {
	  while (count--) {
	    EMU_HANDLER(port).write_portw(port, *dest);
	    dest += incr;
	  }
	}
	if (debug_level('T')) {
		dest = base;
		while (count_--) {
			LOG_PORT_WRITE_W(port, *dest);
			dest += incr;
		}
	}
	return (Bit8u *)dest-(Bit8u *)base;
}

int port_rep_ind(ioport_t port, Bit32u *base, int df, Bit32u count)
{
	register int incr = df? -1: 1;
	Bit32u *dest = base;

	if (count==0) return 0;
	while (count--) {
	  *dest = port_ind(port);
	  LOG_PORT_READ_D(port, *dest);
	  dest += incr;
	}
	return (Bit8u *)dest-(Bit8u *)base;
}

int port_rep_outd(ioport_t port, Bit32u *base, int df, Bit32u count)
{
	register int incr = df? -1: 1;
	Bit32u *dest = base;

	if (count==0) return 0;
	while (count--) {
	  port_outd(port, *dest);
	  LOG_PORT_WRITE_D(port, *dest);
	  dest += incr;
	}
	return (Bit8u *)dest-(Bit8u *)base;
}


/* ---------------------------------------------------------------------- */
/* 
 * SIDOC_BEGIN_FUNCTION special_port_inb,special_port_outb
 *
 * I don't know what to do of this stuff... it was added incrementally to
 * port.c and has mainly to do with video code. This is not the right
 * place for it...
 * Anyway, this implements some HGC stuff for X and the emuretrace
 * port access for 0x3c0/0x3da
 *
 * SIDOC_END_FUNCTION
 */

static int r3da_pending = 0;

void do_r3da_pending (void)
{
  if (r3da_pending) {
    (void)std_port_inb(r3da_pending);
    r3da_pending = 0;
  }
}

static Bit8u special_port_inb(ioport_t port)
{
	Bit8u res = 0xff;

	if (config.usesX) {
	/* HGC stuff */
	    if (port==0x3b8) {	/* HGC mode-reg */
		res = std_port_inb (port);
		return ((res & 0x7f) | (hgc_Mode & 0x80));
	    }
	    else
	    if (port==0x3bf) {	/* HGC conf-reg */
		res = std_port_inb (port);
		return ((res & 0xfd) | (hgc_Konv & 0x02));
	    }
	}
	if ((port==0x3ba)||(port==0x3da)) {
		res = Misc_get_input_status_1();
		if (!config.usesX && !r3da_pending && (config.emuretrace>1)) {
			r3da_pending = port;
		}
	}
	else
	if (port==0x3db)	/* light pen strobe reset */
		res = 0;
	return res;
}

static void special_port_outb(ioport_t port, Bit8u byte)
{
	if (config.usesX) {
	    if (port==0x3b8) {	/* HGC mode-reg */
		if (byte & 0x80) set_hgc_page(1);
			else set_hgc_page(0);
		byte &= 0x7f;
		goto defout;
	    }
	    else
	    if (port==0x3bf) {	/* HGC conf-reg */
		if (byte & 0x02) map_hgc_page(1);
			else map_hgc_page(0);
		byte &= 0xfd;
		goto defout;
	    }
	}

	/* Port writes for enable/disable blinking character mode */
	if (port == 0x03c0) {
		static int last_byte = -1;
		static int last_index = -1;
		static int flip_flop = 1;

		/* SIDOC_BEGIN_REMARK
		 *
		 * This is the core of the new emuretrace algorithm:
		 * If a read of port 0x3da is performed we just set it
		 *  as pending and set ioperm OFF for port 0x3c0
		 * When a write to port 0x3c0 is then trapped, we perform
		 *  any pending read to 0x3da and reset the ioperm for
		 *  0x3c0 in the default ON state.
		 * This way we avoid extra port accesses when the program
		 * is only looking for the sync bits, and we don't miss
		 * the case where the read to 0x3da is used to reset the
		 * index/data flipflop for port 0x3c0. Futher accesses to
		 * port 0x3c0 are handled at full speed.
		 *
		 * SIDOC_END_REMARK
		 */
		if (config.vga && !config.usesX && (config.emuretrace>1)) {
		    if (r3da_pending) {
			(void)std_port_inb(r3da_pending);
			r3da_pending = 0;
			std_port_outb(0x3c0, byte);
			return;
		    }
		    goto defout;
		}
		flip_flop = !flip_flop;
		if (flip_flop) {
		/* JES This was last_index = 0x10..... WRONG? */
			if (last_index == 0x10)
				char_blink = (byte & 8) ? 1 : 0;
			last_byte = byte;
		}
		else {
			last_index = byte;
		}
		return;
	}

	/* Port writes for cursor position: 3b4-3b5 or 3d4-3d5 */
	if (!config.usesX && ((port & 0xfffe)==READ_WORD(BIOS_VIDEO_PORT))) {
		/* Writing to the 6845 */
		static int last_port;
		static int last_byte;
		static unsigned int hi = 0, lo = 0;
		int pos;

		v_printf("PORT: 6845 outb [0x%04x]\n", port);
		if ((port == READ_WORD(BIOS_VIDEO_PORT) + 1) && (last_port == READ_WORD(BIOS_VIDEO_PORT))) {
			/* We only take care of cursor positioning for now. */
			/* This code should work most of the time, but can
			   be defeated if i/o permissions change (e.g. by a vt
			   switch) while a new cursor location is being written
			   to the 6845. */
			if (last_byte == 14) {
				hi = byte;
				pos = (hi << 8) | lo;
				cursor_col = pos % 80;
				cursor_row = pos / 80;
			}
			else if (last_byte == 15) {
				lo = byte;
				pos = (hi << 8) | lo;
				cursor_col = pos % 80;
				cursor_row = pos / 80;
			}
		}
		last_port = port;
		last_byte = byte;
		return;
	}
defout:
	std_port_outb (port, byte);
}

/* ---------------------------------------------------------------------- */
/* 
 * SIDOC_BEGIN_FUNCTION port_init()
 *
 * Resets all the port port_handler information.
 * This must be called before parsing the config file -
 * This must NOT be called again when warm booting!
 * Can't use debug logging, it is called too early.
 *
 * SIDOC_END_FUNCTION
 */
int port_init(void)
{
  int i;
	/* set unused elements to appropriate values */
	for (i=0; i < EMU_MAX_IO_DEVICES; i++) {
	  port_handler[i].read_portb   = NULL;
	  port_handler[i].write_portb  = NULL;
	  port_handler[i].read_portw   = NULL;
	  port_handler[i].write_portw  = NULL;
	  port_handler[i].read_portd   = NULL;
	  port_handler[i].write_portd  = NULL;
	  port_handler[i].irq = EMU_NO_IRQ;
	  port_handler[i].fd = -1;
	}
#ifdef X86_EMULATOR
	memset (io_bitmap, 0, sizeof(io_bitmap));
#endif

  /* handle 0 maps to the unmapped IO device handler.  Basically any
     ports which don't map to any other device get mapped to this
     handler which does absolutely nothing.
   */
	port_handler[NO_HANDLE].read_portb = port_not_avail_inb;
	port_handler[NO_HANDLE].write_portb = port_not_avail_outb;
	port_handler[NO_HANDLE].read_portw = port_not_avail_inw;
	port_handler[NO_HANDLE].write_portw = port_not_avail_outw;
	port_handler[NO_HANDLE].read_portd = port_not_avail_ind;
	port_handler[NO_HANDLE].write_portd = port_not_avail_outd;
	port_handler[NO_HANDLE].handler_name = "unknown port";

  /* the STD handles will be in use by many devices, and their fd
     will always be -1
   */
	port_handler[HANDLE_STD_IO].read_portb = std_port_inb;
	port_handler[HANDLE_STD_IO].write_portb = std_port_outb;
	port_handler[HANDLE_STD_IO].read_portw = std_port_inw;
	port_handler[HANDLE_STD_IO].write_portw = std_port_outw;
	port_handler[HANDLE_STD_IO].read_portd = std_port_ind;
	port_handler[HANDLE_STD_IO].write_portd = std_port_outd;
	port_handler[HANDLE_STD_IO].handler_name = "std port io";

	port_handler[HANDLE_STD_RD].read_portb = std_port_inb;
	port_handler[HANDLE_STD_RD].write_portb = port_not_avail_outb;
	port_handler[HANDLE_STD_RD].read_portw = std_port_inw;
	port_handler[HANDLE_STD_RD].write_portw = port_not_avail_outw;
	port_handler[HANDLE_STD_RD].read_portd = std_port_ind;
	port_handler[HANDLE_STD_RD].write_portd = port_not_avail_outd;
	port_handler[HANDLE_STD_RD].handler_name = "std port read";

	port_handler[HANDLE_STD_WR].read_portb = port_not_avail_inb;
	port_handler[HANDLE_STD_WR].write_portb = std_port_outb;
	port_handler[HANDLE_STD_WR].read_portw = port_not_avail_inw;
	port_handler[HANDLE_STD_WR].write_portw = std_port_outw;
	port_handler[HANDLE_STD_WR].read_portd = port_not_avail_ind;
	port_handler[HANDLE_STD_WR].write_portd = std_port_outd;
	port_handler[HANDLE_STD_WR].handler_name = "std port write";

	port_handler[HANDLE_VID_IO].read_portb = video_port_in;
	port_handler[HANDLE_VID_IO].write_portb = video_port_out;
	port_handler[HANDLE_VID_IO].read_portw = port_not_avail_inw;
	port_handler[HANDLE_VID_IO].write_portw = port_not_avail_outw;
	port_handler[HANDLE_VID_IO].read_portd = port_not_avail_ind;
	port_handler[HANDLE_VID_IO].write_portd = port_not_avail_outd;
	port_handler[HANDLE_VID_IO].handler_name = "video port io";

	port_handler[HANDLE_SPECIAL].read_portb = special_port_inb;
	port_handler[HANDLE_SPECIAL].write_portb = special_port_outb;
	port_handler[HANDLE_SPECIAL].read_portw = port_not_avail_inw;
	port_handler[HANDLE_SPECIAL].write_portw = port_not_avail_outw;
	port_handler[HANDLE_SPECIAL].read_portd = port_not_avail_ind;
	port_handler[HANDLE_SPECIAL].write_portd = port_not_avail_outd;
	port_handler[HANDLE_SPECIAL].handler_name = "extra stuff";

	port_handles = STD_HANDLES;

	memset (port_handle_table, NO_HANDLE, sizeof(port_handle_table));
	memset (port_andmask, 0xff, sizeof(port_andmask));
	memset (port_ormask, 0, sizeof(port_ormask));

	return port_handles;	/* unused but useful */
}

/* port server: this function runs in a seperate process from the main
   DOSEMU. This enables the main DOSEMU to drop root privileges. The
   server can do that as well: by setting iopl(3).
   Maybe this server should wrap DOSEMU rather than be forked from
   it.
*/
void port_server(void)
{
        struct portreq pr;
        unsigned char port_type;
        priv_iopl(3);
	priv_drop();
        close(port_fd_in[0]);
        close(port_fd_out[1]);
        g_printf("server started\n");
        for (;;) {
                read(port_fd_out[0], &pr, sizeof(pr));
                if (pr.type >= TYPE_EXIT)
                        exit((int)pr.word);
                port_type = port_handle_table[pr.port];
                if (port_type != 0 && port_type < STD_HANDLES)
                switch (pr.type) {
                case TYPE_INB:
                        if(port_handler[port_type].read_portb !=
                           port_not_avail_inb)
                                pr.word = port_real_inb(pr.port);
                        break;
                case TYPE_OUTB:
                        if(port_handler[port_type].write_portb !=
                           port_not_avail_outb)
                                port_real_outb(pr.port, pr.word);
                        break;
                case TYPE_INW:
                        if(port_handler[port_type].read_portw !=
                           port_not_avail_inw)                        
                                pr.word = port_real_inw(pr.port);
                        break;
                case TYPE_OUTW:
                        if(port_handler[port_type].write_portw !=
                           port_not_avail_outw)
                                port_real_outw(pr.port, pr.word);
                        break;
                case TYPE_IND:
                        if(port_handler[port_type].read_portd !=
                           port_not_avail_ind)
                                pr.word = port_real_ind(pr.port);
                        break;
                case TYPE_OUTD:
                        if(port_handler[port_type].write_portd !=
                           port_not_avail_outd)
                                port_real_outd(pr.port, pr.word);
                        break;
                }
                if (!(pr.type & 1))
                        write(port_fd_in[1], &pr, sizeof(pr));
        }
}

/* 
 * SIDOC_BEGIN_FUNCTION extra_port_init()
 *
 * Catch all the special cases previously defined in ports.c
 * mainly video stuff that should be moved away from here
 * This must be called at the end of initialization phase
 *
 * NOTE: the order in which these inits are done could be significant!
 *   I tried to keep it the same it was in ports.c but this code surely
 *   can still have bugs
 *
 * SIDOC_END_FUNCTION
 */
int extra_port_init(void)
{
  int i, j;
/*
 * DANG_FIXTHIS This stuff should be moved to video code!!
 */
	if (config.chipset == ATI) {	/* taken literally! */
		SET_HANDLE(0x46e8,HANDLE_STD_IO);
		for (i=0x400; i<0x10000; i+=0x400) {
			SET_HANDLE(i+0x102,HANDLE_STD_IO);
			SET_HANDLE(i+0x1ce,HANDLE_STD_IO);
			SET_HANDLE(i+0x1cf,HANDLE_STD_IO);
			for (j=0; j<4; j++)
				SET_HANDLE(i+j+0x2ec,HANDLE_STD_IO);
			SET_HANDLE(i+0x3b4,HANDLE_STD_IO);
			SET_HANDLE(i+0x3b5,HANDLE_STD_IO);
			for (j=0; j<4; j++)
				SET_HANDLE(i+j+0x3b8,HANDLE_STD_IO);
			for (j=0; j<11; j++)
				SET_HANDLE(i+j+0x3c0,HANDLE_STD_IO);
			SET_HANDLE(i+0x3cc,HANDLE_STD_IO);
			SET_HANDLE(i+0x3ce,HANDLE_STD_IO);
			SET_HANDLE(i+0x3cf,HANDLE_STD_IO);
			SET_HANDLE(i+0x3d4,HANDLE_STD_IO);
			SET_HANDLE(i+0x3d5,HANDLE_STD_IO);
			for (j=0; j<5; j++)
				SET_HANDLE(i+j+0x3d8,HANDLE_STD_IO);
		}
	}
	if (config.chipset == WDVGA) {
		SET_HANDLE(0x46e8,HANDLE_STD_IO);
		SET_HANDLE(0x4ae8,HANDLE_STD_IO);
		SET_HANDLE(0xe92,HANDLE_STD_IO);	/* ??? */
		SET_HANDLE(0xef2,HANDLE_STD_IO);	/* ??? */
		SET_HANDLE(0xa875,HANDLE_STD_IO);	/* ??? */
		for (i=0x2df0; i<0x2df4; i++)
			SET_HANDLE(i,HANDLE_STD_IO);
	}
	if ((config.chipset == DIAMOND) ||	/* the DIAMOND bug */
	    (config.chipset == WDVGA)) {
		for (i=0x23c0; i<0x23d0; i++)
			SET_HANDLE(i,HANDLE_STD_IO);
	}

	if (v_8514_base) {
		for (i=v_8514_base+0x400; i<0x10000; i+=0x400) {
			SET_HANDLE_COND(i,HANDLE_STD_IO);
			SET_HANDLE_COND(i+1,HANDLE_STD_IO);
		}
	}
	if (config.chipset && config.mapped_bios) {
		for (i=0x3b4; i<0x3df; i++)
			SET_HANDLE_COND(i,HANDLE_VID_IO);
	}
#if 1		/* HGC ports */
	if (config.usesX) {
		SET_HANDLE_COND(0x3b4,HANDLE_STD_IO);
		SET_HANDLE_COND(0x3b5,HANDLE_STD_IO);
		SET_HANDLE_COND(0x3ba,HANDLE_STD_IO);
	}
#endif

	if (!config.X) {
 	  SET_HANDLE_COND(0x3b8,HANDLE_SPECIAL);
	  SET_HANDLE_COND(0x3bf,HANDLE_SPECIAL);
	  SET_HANDLE_COND(0x3c0,HANDLE_SPECIAL);	/* W */
  	  SET_HANDLE_COND(0x3ba,HANDLE_SPECIAL);		/* R */
	  SET_HANDLE_COND(0x3da,HANDLE_SPECIAL);		/* R */
	  SET_HANDLE_COND(0x3db,HANDLE_SPECIAL);		/* R */
	}

	i = READ_WORD(BIOS_VIDEO_PORT);
	if (i && !config.X) {	/* !config.vga */
	    SET_HANDLE_COND(i,HANDLE_SPECIAL);		/* W */
	    SET_HANDLE_COND(i+1,HANDLE_SPECIAL);	/* W */
	}

        if (can_do_root_stuff) {
                for (i = 0; i < sizeof(port_handle_table); i++) {
                        if (port_handle_table[i] >= HANDLE_STD_IO &&
                            port_handle_table[i] <= HANDLE_STD_WR) {
                                /* fork the privileged port server */
                                g_printf("starting port server\n");
                                pipe(port_fd_out);
                                pipe(port_fd_in);
                                if (fork() == 0) {
                                        port_server();
                                }
                                close(port_fd_in[1]);
                                close(port_fd_out[0]);
                                break;
                        }
                }
        }

 	return 0;
}

void port_exit(int sig)
{
        struct portreq pr;
        pr.type = TYPE_EXIT;
        pr.word = (unsigned long)sig;
        if (port_fd_out[1]) write(port_fd_out[1], &pr, sizeof(pr));
}

void release_ports (void)
{
	int i;

	for (i=0; i < port_handles; i++) {
		if (port_handler[i].fd >= 2) {
			close(port_handler[i].fd);
/* DANG_FIXTHIS we should free the name but we are going to exit anyway
 */
/*			free(port_handler[i].handler_name); */
		}
	}
	memset (port_handle_table, NO_HANDLE, sizeof(port_handle_table));
	memset (port_andmask, 0xff, sizeof(port_andmask));
	memset (port_ormask, 0, sizeof(port_ormask));

  }

/* ---------------------------------------------------------------------- */
/* 
 * SIDOC_BEGIN_FUNCTION port_register_handler
 *
 * Assigns a handle in the port table to a range of ports with or
 * without a device, and registers the ports
 *
 * SIDOC_END_FUNCTION
 */
int port_register_handler(emu_iodev_t device, int flags)
{
    int handle, i;
    struct stat devstat;

    if (device.irq != EMU_NO_IRQ && device.irq >= EMU_MAX_IRQS) {
	dbug_printf("PORT: IO device %s registered with IRQ=%d above %u\n",
	      device.handler_name, device.irq, EMU_MAX_IRQS - 1);
	return 1;
    }

    /* first find existing handle for function or create new one */
    handle = port_handles;
    for (handle=0; handle < port_handles; handle++) {
	if (!strcmp(port_handler[handle].handler_name, device.handler_name))
	      break;
    }

    if (handle >= port_handles) {
	/* no existing handle found, create new one */
	if (port_handles >= EMU_MAX_IO_DEVICES) {
		error("PORT: too many IO devices, increase EMU_MAX_IO_DEVICES");
		leavedos(77);
	}

	if (device.irq != EMU_NO_IRQ && irq_handler_name[device.irq]) {
		error("PORT: IRQ %d conflict.  IO devices %s & %s\n",
		      device.irq, irq_handler_name[device.irq], device.handler_name);
		if (device.fd) close(device.fd);
		return 2;
	}
	if (device.irq != EMU_NO_IRQ && device.irq < EMU_MAX_IRQS)
		irq_handler_name[device.irq] = device.handler_name;
	port_handles++;

	/*
	 * for byte and double, a NULL function means that the port
	 * access is not available, while for word means that it will
	 * be translated into 2 byte accesses
	 */
	port_handler[handle].read_portb = 
		(device.read_portb? : port_not_avail_inb);
	port_handler[handle].write_portb = 
		(device.write_portb? : port_not_avail_outb);
	port_handler[handle].read_portw = device.read_portw;
	port_handler[handle].write_portw = device.write_portw;
	port_handler[handle].read_portd =
		(device.read_portd? : port_not_avail_ind);
	port_handler[handle].write_portd =
		(device.write_portd? : port_not_avail_outd);
	port_handler[handle].handler_name = device.handler_name;
	port_handler[handle].irq = device.irq;
	port_handler[handle].fd = -1;
    }

  /* change table to reflect new handler id for that address */
    for (i = device.start_addr; i <= device.end_addr; i++) {
	if (port_handle_table[i] != 0) {
		error("PORT: conflicting devices: %s & %s\n",
		      port_handler[handle].handler_name, EMU_HANDLER(i).handler_name);
		if (device.fd) close(device.fd);
		return 4;
	}
	port_handle_table[i] = handle;
    }

    i_printf("PORT: registered \"%s\" handle 0x%02x [0x%04x-0x%04x] fd=%d\n",
	port_handler[handle].handler_name, handle, device.start_addr,
	device.end_addr, (int)(device.fd>=0? devstat.st_dev:device.fd));

    if (flags & PORT_FAST) {
	if (device.start_addr < 0x400) {
	  i_printf("PORT: giving fast access to ports [0x%04x-0x%04x]\n",
		device.start_addr, device.end_addr);
	  set_ioperm (device.start_addr, device.end_addr-device.start_addr+1, 1);
	}
	else
	  i_printf("PORT: using perm/iopl for ports [0x%04x-0x%04x]\n",
		device.start_addr, device.end_addr);
    }
    return 0;
}


/* 
 * SIDOC_BEGIN_FUNCTION port_allow_io
 *
 *
 * SIDOC_END_FUNCTION
 */
Boolean port_allow_io(ioport_t start, Bit16u size, int permission, Bit8u ormask,
	Bit8u andmask, unsigned int flags, char *device)
{
	static emu_iodev_t io_device;
	FILE *fp;
	unsigned int beg, end;
	char line[100], portname[50], lock_file[64];
	unsigned char mapped;
	char *devrname;
	int fd, usemasks = 0;

        if (!can_do_root_stuff)
                return FALSE;

	i_printf("PORT: allow_io for port 0x%04x:%d perm=%x or=%x and=%x\n",
		 start, size, permission, ormask, andmask);

	if ((ormask != 0) || (andmask != 0xff)) {
		if ((start+size) > 0x400)
			i_printf("PORT: andmask & ormask not supported for ports>=0x400\n");
		else if (size > 1)
			i_printf("PORT: andmask & ormask not supported for multiple ports\n");
		else
			usemasks = 1;
	}

	/* SIDOC_BEGIN_REMARK
	 * find out whether the port address request is available;
	 * this way, try to deny uncoordinated access
	 *
	 * If it is not listed in /proc/ioports, register them 
	 * (we need some syscall to do so bo 960609)...
	 * (we have a module to do so AV 970813)
	 * if it is registered, we need the name of a device to open
	 * if we can't open it, we disallow access to that port
	 * SIDOC_END_REMARK
	 */
	if ((fp = fopen("/proc/ioports", "r")) == NULL) {
		i_printf("PORT: can't open /proc/ioports\n");
		return FALSE;
	}
	mapped = 0;
	while (fgets(line, 100, fp)) {
		sscanf(line, "%x-%x : %s", &beg, &end, portname);
		if ((start <= end) && ((start+size) > beg)) {
			/* ports are besetzt, try to open the according device */
			i_printf("PORT: range 0x%04x-0x%04x already registered as %s\n",
				 beg, end, portname);
			if (!strncasecmp(portname,"dosemu",6)) return FALSE;
			mapped = 1;
			break;
		}
	}
	fclose (fp);

	if (mapped && ((device==NULL) || (*device==0))) {
		i_printf ("PORT: no device specified for %s\n", portname);
		return FALSE;
	}

	io_device.fd = -1;
	if (device && *device) {
		/* SIDOC_BEGIN_REMARK
		 * We need to check if our required port range is in use
		 * by some device. So we look into proc/ioports to check
		 * the addresses. Fine, but at this point we must supply
		 * a device name ourselves, and we can't check from here
		 * if it's the right one. The device is then open and left
		 * open until dosemu ends; for the rest, in the original
		 * code the device wasn't used, just locked, and only then
		 * port access was granted.
		 * SIDOC_END_REMARK
		 */
		int devperm;

		devrname=strrchr(device,'/');
		if (devrname==NULL) devrname=device; else devrname++;
		sprintf(lock_file, "%s/%s%s", PATH_LOCKD, NAME_LOCKF, devrname);

		switch (permission) {
			case IO_READ:	devperm = O_RDONLY;
					flags |= PORT_DEV_RD;
					break;
			case IO_WRITE:	devperm = O_WRONLY;
					flags |= PORT_DEV_WR;
					break;
			default:	devperm = O_RDWR;
					flags |= (PORT_DEV_RD|PORT_DEV_WR);
		}
		io_device.fd = open(device, devperm);
		if (io_device.fd == -1) {
			switch (errno) {
			case EBUSY:
				i_printf("PORT: Device %s busy\n", device);
				return FALSE;
			case EACCES:
				i_printf("PORT: Device %s, access not allowed\n", device);
				return FALSE;
			case ENOENT:
			case ENXIO:
				i_printf("PORT: No such Device '%s'\n", device);
				return FALSE;
			default:
				i_printf("PORT: Device %s error %d\n", device, errno);
				return FALSE;
			}
		}

		fd = open(lock_file, O_RDONLY);
		if (fd >= 0) {
			close(fd);
			i_printf("PORT: Device %s is locked\n", device);
			return FALSE;
		}

		i_printf("PORT: Device %s opened successfully = %d\n", device,
			io_device.fd);

	}

	if (permission == IO_RDWR)
		io_device.handler_name = "std port io";
	else if (permission == IO_READ)
		io_device.handler_name = "std port read";
	else
		io_device.handler_name = "std port write";

	io_device.start_addr   = start;
	io_device.end_addr     = start + size - 1;
	io_device.irq          = EMU_NO_IRQ;

	if (usemasks) {
		port_andmask[start] = andmask;
		port_ormask[start] = ormask;
	}
	port_register_handler(io_device, flags);
	return TRUE;
}

/* 
 * SIDOC_BEGIN_FUNCTION set_ioperm
 *
 * wrapper for the ioperm() syscall, returns -1 if port>=0x400
 *
 * SIDOC_END_FUNCTION
 */
int
set_ioperm(int start, int size, int flag)
{
	PRIV_SAVE_AREA
	int tmp;

	if ((!can_do_root_stuff && flag == 1) || (start>0x3ff))
	    return -1;		/* don't bother */
	if ((start+size)>0x3ff) size=0x400-start;

	/* While possibly not the best behavior I figure we ought to,
	   turn the privilege on here instead of in every caller.
	   If we want a privileged version of this function we can
	   call ioperm.
	 */
	enter_priv_on();
	tmp = DOS_SYSCALL(ioperm(start, size, flag));
	leave_priv_setting();

#ifdef X86_EMULATOR
	if (config.cpuemu && (tmp==0)) {
	    int i;
	    for (i=start; i<(start+size); i++)
		(flag? set_bit(i,io_bitmap) : clear_bit(i,io_bitmap));
	    i_printf("ePORT: set_ioperm [%x:%d:%d]\n",start,size,flag);
	}
#else
	i_printf ("nPORT: set_ioperm [%x:%d:%d] returns %d\n",start,size,flag,tmp);
#endif
	return tmp;
}


/* ====================================================================== */


