/* dos emulator, Matthias Lautner */
#ifndef EMU_H
#define EMU_H
/* Extensions by Robert Sanders, 1992-93
 *
 * $Date: 1994/02/10 20:41:14 $
 * $Source: /home/src/dosemu0.49pl4g/RCS/emu.h,v $
 * $Revision: 1.13 $
 * $State: Exp $
 *
 * $Log: emu.h,v $
 * Revision 1.13  1994/02/10  20:41:14  root
 * Last cleanup prior to release of pl4.
 *
 * Revision 1.12  1994/02/09  20:10:24  root
 * Added dosbanner config option for optionally displaying dosemu bannerinfo.
 * Added allowvideportaccess config option to deal with video ports.
 *
 * Revision 1.11  1994/02/05  21:45:55  root
 * Minor bugfixes [2.
 *
 * Revision 1.10  1994/02/01  20:57:31  root
 * With unlimited thanks to gorden@jegnixa.hsc.missouri.edu (Jason Gorden),
 * here's a packet driver to compliment Tim_R_Bird@Novell.COM's IPX work.[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[his packet driver  to compliment Tim_R_Bird@Novell.COM's IPX work.
 *
 * Revision 1.9  1994/01/31  18:27:21  root
 * Mods for first round of terminfo intergration.
 *
 * Revision 1.8  1994/01/27  21:47:09  root
 * Patches by Tim_R_Bird@Novell.COM in preparation for IPX under DOSEMU.
 *
 * Revision 1.7  1994/01/27  19:43:54  root
 * Prepare for Tim's IPX implementation.
 *
 * Revision 1.6  1994/01/25  20:02:44  root
 * Added D_printf for DPMI debug messages.
 * Exchange stderr <-> stdout.
 *
 * Revision 1.5  1994/01/20  21:14:24  root
 * Indent.
 *
 * Revision 1.4  1993/12/30  11:18:32  root
 * Theadore T'so's changes to allow booting from a bootdisk and returning
 * the floppy for dosemu to use.
 *
 * Revision 1.3  1993/11/30  21:26:44  root
 * Chips First set of patches, WOW!
 *
 * Revision 1.2  1993/11/23  22:24:53  root
 * Work on serial to 9600
 *
 * Revision 1.1  1993/11/12  12:32:17  root
 * Initial revision
 *
 * Revision 1.2  1993/07/07  21:42:04  root
 * minor changes for -Wall
 *
 * Revision 1.1  1993/07/07  00:49:06  root
 * Initial revision
 *
 * Revision 1.13  1993/05/04  05:29:22  root
 * added console switching, new parse commands, and serial emulation
 *
 * Revision 1.12  1993/04/05  17:25:13  root
 * big pre-49 checkit; EMS, new MFS redirector, etc.
 *
 * Revision 1.11  1993/03/02  03:06:42  root
 * somewhere between 0.48pl1 and 0.49 (with IPC).  added virtual IOPL
 * and AC support (for 386/486 tests), -3 and -4 flags for choosing.
 * Split dosemu into 2 processes; the child select()s on the keyboard,
 * and signals the parent when a key is received (also sends it on a
 * UNIX domain socket...this might not work well for non-console keyb).
 *
 * Revision 1.10  1993/02/24  11:33:24  root
 * some general cleanups, fixed the key-repeat bug.
 *
 * Revision 1.9  1993/02/18  19:35:58  root
 * just added newline so diff wouldn't barf
 *
 * Revision 1.8  1993/02/13  23:37:20  root
 * latest version, no time to document!
 *
 * Revision 1.7  1993/02/10  20:56:45  root
 * for the circa-Wp dosemu
 *
 * Revision 1.6  1993/02/08  04:17:56  root
 * dosemu 0.47.7
 *
 * Revision 1.5  1993/02/05  02:54:24  root
 * this is for 0.47.6
 *
 * Revision 1.4  1993/02/04  01:16:57  root
 * version 0.47.5
 *
 * Revision 1.3  1993/01/28  02:19:59  root
 * for emu.c 1.12
 * THIS IS THE DOSEMU47 DISTRIBUTION EMU.H
 *
 */

#include "cpu.h"
#include <sys/types.h>

#define BIT(x)  	(1<<x)

#define us unsigned short

extern struct vm86_struct vm86s;
extern int screen, max_page, screen_mode, update_screen, scrtest_bitmap;

extern char *cl,		/* clear screen */
*le,				/* cursor left */
*cm,				/* goto */
*ce,				/* clear to end */
*sr,				/* scroll reverse */
*so,				/* stand out start */
*se,				/* stand out end */
*md,				/* hilighted */
*mr,				/* reverse */
*me,				/* normal */
*ti,				/* terminal init */
*te,				/* terminal exit */
*ks,				/* init keys */
*ke,				/* ens keys */
*vi,				/* cursor invisible */
*ve;				/* cursor normal */

extern int kbd_fd, mem_fd, ioc_fd;
extern int in_readkeyboard;

extern int in_vm86;

extern int li, co, li2, co2;	/* lines, columns */
extern int lastscan, scanseq;

/* #define CO	80
   #define LI	25 */

/* would use the info termio.c nicely got for us, but it works badly now */
#define CO	co2
#define LI	li2

/* these are flags to char_out() and char_out_attr()...specify whether the
 * cursor whould be addressed
 */
#define ADVANCE		1
#define NO_ADVANCE	0

void dos_ctrlc(void), dos_ctrl_alt_del(void);
void show_regs(void);
int ext_fs(int, char *, char *, int);
void char_out_att(u_char, u_char, int, int);
int outch(int c);
void termioInit();
void termioClose();
inline void run_vm86(void);

#define NOWAIT  0
#define WAIT    1
#define TEST    2
#define POLL    3

int ReadKeyboard(unsigned int *, int);
int CReadKeyboard(unsigned int *, int);
int InsKeyboard(unsigned short scancode);
int PollKeyboard(void);
void ReadString(int, unsigned char *);

inline static void 
port_out(char value, unsigned short port)
{
  __asm__ volatile ("outb %0,%1"
		    ::"a" ((char) value), "d"((unsigned short) port));
}

inline static char 
port_in(unsigned short port)
{
  char _v;
  __asm__ volatile ("inb %1,%0"
		    :"=a" (_v):"d"((unsigned short) port));

  return _v;
}

struct debug_flags {
  unsigned char
   disk,			/* disk msgs, "d" */
   read,			/* disk read "R" */
   write,			/* disk write "W" */
   dos,				/* unparsed int 21h, "D" */
   video,			/* video, "v" */
   keyb,			/* keyboard, "k" */
   debug, io,			/* port I/O, "i" */
   serial,			/* serial, "s" */
   defint,			/* default ints */
   printer, general, warning, all,	/* all non-classifiable messages */
   hardware, xms, mouse, IPC, EMS, config, dpmi, network;     /* TRB - only IPX for now */
};

#if __GNUC__ >= 2
# define FORMAT(T,A,B)  __attribute__((format(T,A,B)))
#else
# define FORMAT(T,A,B)
#endif

extern void saytime();

int 
ifprintf(unsigned char, const char *,...) FORMAT(printf, 2, 3);
void p_dos_str(char *,...) FORMAT(printf, 1, 2);

#if 1

#define dbug_printf(f,a...)	ifprintf(2,f,##a)
#define k_printf(f,a...) 	ifprintf(d.keyb,f,##a)
#define h_printf(f,a...) 	ifprintf(d.hardware,f,##a)
#define v_printf(f,a...) 	ifprintf(d.video,f,##a)
#define s_printf(f,a...) 	ifprintf(d.serial,f,##a)
#define p_printf(f,a...) 	ifprintf(d.printer,f,##a)
#define d_printf(f,a...) 	ifprintf(d.disk,f,##a)
#define i_printf(f,a...) 	ifprintf(d.io,f,##a)
#define R_printf(f,a...) 	ifprintf(d.read,f,##a)
#define W_printf(f,a...) 	ifprintf(d.write,f,##a)
#define warn(f,a...)     	ifprintf(d.warning,f,##a)
#define g_printf(f,a...)	ifprintf(d.general,f,##a)
#define x_printf(f,a...)	ifprintf(d.xms,f,##a)
#define D_printf(f,a...)	ifprintf(d.dpmi,f,##a)
#define m_printf(f,a...)	ifprintf(d.mouse,f,##a)
#define I_printf(f,a...) 	ifprintf(d.IPC,f,##a)
#define E_printf(f,a...) 	ifprintf(d.EMS,f,##a)
#define c_printf(f,a...) 	ifprintf(d.config,f,##a)
#define e_printf(f,a...) 	ifprintf(1,f,##a)
#define n_printf(f,a...)        ifprintf(d.network,f,##a)       /* TRB */
#define pd_printf(f,a...)       ifprintf(0,f,##a)          /* pktdrvr  */
#define error(f,a...)	 	ifprintf(1,f,##a)

#else
#define dbug_printf(f,a...)	ifprintf(2,f,##a)
#define k_printf(f,a...) 
#define h_printf(f,a...)
#define v_printf(f,a...)
#define s_printf(f,a...)
#define p_printf(f,a...)
#define d_printf(f,a...)
#define i_printf(f,a...) 
#define R_printf(f,a...)
#define W_printf(f,a...)
#define warn(f,a...)    
#define g_printf(f,a...)
#define x_printf(f,a...)
#define D_printf(f,a...)
#define m_printf(f,a...)
#define I_printf(f,a...)
#define E_printf(f,a...)
#define c_printf(f,a...)
#define e_printf(f,a...)
#define n_printf(f,a...)
#define pd_printf(f,a...)  
#define error(f,a...)	 	

#endif
     /* #define char_out(c,s,af)   char_out_att(c,7,s,af) */
     void char_out(u_char, int, int);

     struct ioctlq {
       int fd, req, param3;
       int queued;
     };

     void do_queued_ioctl(void);
     int queue_ioctl(int, int, int), do_ioctl(int, int, int);
     void keybuf_clear(void);

     int set_ioperm(int, int, int);

#ifndef EMU_C
     extern struct debug_flags d;
     extern int gfx_mode;	/* flag for in gxf mode or not */
     extern int in_sighandler, in_ioctl;
     extern struct ioctlq iq, curi;

#endif

     /* int 11h config single bit tests
 */
#define CONF_FLOP	BIT(0)
#define CONF_MATHCO	BIT(1)
#define CONF_MOUSE	BIT(2)
#define CONF_DMA	BIT(8)
#define CONF_GAME	BIT(12)

     /* don't use CONF_NSER with num > 4, CONF_NLPT with num > 3, CONF_NFLOP
 * with num > 4
 */
#define CONF_NSER(c,num)	{c&=~(BIT(9)|BIT(10)|BIT(11)); c|=(num<<9);}
#define CONF_NLPT(c,num) 	{c&=~(BIT(14)|BIT(14)); c|=(num<<14);}
#define CONF_NFLOP(c,num) 	{c&=~(CONF_FLOP|BIT(6)|BIT(7)); \
				   if (num) c|=((num-1)<<6)|CONF_FLOP;}

     /* initial reported video mode */
#ifdef MDA_VIDEO
#define INIT_SCREEN_MODE	7	/* 80x25 MDA monochrome */
#define CONF_SCRMODE		(3<<4)	/* for int 11h info */
#define VID_COMBO		1
#define VID_SUBSYS		1	/* 1=mono */
#define BASE_CRTC		0x3b4
#else
#define INIT_SCREEN_MODE	3	/* 80x25 VGA color */
#define CONF_SCRMODE		(2<<4)	/* (2<<4)=80x25 color CGA, 0=EGA/VGA */
#define VID_COMBO		4	/* 4=EGA (ok), 8=VGA (not ok??) */
#define VID_SUBSYS		0	/* 0=color */
#define BASE_CRTC		0x3d4
#endif

     /* this macro can be safely wrapped around a system call with no side
 * effects; using a featuer of GCC, it returns the same value as the
 * function call argument inside.
 *
 * this is best used in places where the errors can't be sanely handled,
 * or are not expected...
 */
#define DOS_SYSCALL(sc) ({ int s_tmp = (int)sc; \
  if (s_tmp == -1) \
    error("SYSCALL ERROR: %d, *%s* in file %s, line %d: expr=\n\t%s\n", \
	  errno, strerror(errno), __FILE__, __LINE__, #sc); \
  s_tmp; })

#define RPT_SYSCALL(sc) ({ int s_tmp; \
   do { \
	  s_tmp = sc; \
      } while ((s_tmp == -1) && (errno == EINTR)); \
  s_tmp; })

#define RPT_SYSCALL2(sc) ({ int s_tmp; \
   do { \
	  s_tmp = sc; \
      } while ((s_tmp == -1) ); \
  s_tmp; })

#ifndef USE_NCURSES
#define FALSE	0
#define TRUE	1
#endif

     typedef unsigned char boolean;

     typedef struct config_info {
       int hdiskboot;

       /* for video */
       boolean console_video;
       boolean graphics;
       boolean vga;
       u_short cardtype;
       u_short chipset;
       u_short gfxmemsize;	/* for SVGA card, in K */
       u_short redraw_chunks;
       boolean fullrestore;

       boolean console_keyb;
       boolean exitearly;
       boolean mathco;
       boolean keybint;
       boolean dosbanner;
       boolean allowvideoportaccess;
       boolean timers;
       boolean mouse_flag;
       boolean mapped_bios;	/* video BIOS */
       boolean mapped_sbios;	/* system BIOS */
       char *vbios_file;	/* loaded VBIOS file */
       boolean vbios_copy;

       boolean bootdisk;		/* Special bootdisk defined */
       boolean fastfloppy;
       char *emusys;		/* map CONFIG.SYS to CONFIG.EMU */
       char *emubat;		/* map AUTOEXEC.BAT to AUTOEXEC.EMU */

       u_short speaker;		/* 0 off, 1 native, 2 emulated */
       u_short fdisks, hdisks;
       u_short num_lpt;
       u_short num_ser;

       unsigned int update, freq;	/* temp timer magic */

       unsigned int hogthreshold;

       int mem_size, xms_size, ems_size, dpmi_size;
     }

config_t;

#define SPKR_OFF	0
#define SPKR_NATIVE	1
#define SPKR_EMULATED	2

#define XPOS(s)		(*(u_char *)(0x450 + 2*(s)))
#define YPOS(s)		(*(u_char *)(0x451 + 2*(s)))
#define VIDMODE		(*(u_char *)0x449)
#define SCREEN		(*(u_char *)0x462)
#define COLS		(*(u_short *)0x44a)
#define PAGE_OFFSET	(*(u_short *)0x44e)
#define CUR_START	(*(u_short *)0x461)
#define CUR_END		(*(u_short *)0x461)
#define ROWSM1		(*(u_char *)0x484)	/* rows on screen minus 1 */
#define REGEN_SIZE	(*(u_short *)0x44c)

#ifndef OLD_SCROLL
#define scrollup(x0,y0,x1,y1,l,att) Scroll(x0,y0,x1,y1,l,att)
#define scrolldn(x0,y0,x1,y1,l,att) Scroll(x0,y0,x1,y1,-(l),att)
#endif

     /*
 * Right now, dosemu only supports two serial ports.
 */
#define MAX_SER 2

#define SIG_SER		SIGTTIN

#define SIG_TIME	SIGALRM
#define TIMER_TIME	ITIMER_REAL

#define IO_READ  1
#define IO_WRITE 2
#define IO_RDWR	 (IO_READ | IO_WRITE)

#endif /* EMU_H */
