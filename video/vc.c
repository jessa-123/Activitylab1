/*
 * DANG_BEGIN_MODULE
 *
 * Here's all the calls to the code to try and properly save & restore
 * the video state between VC's and the attempts to control updates to
 * the VC whilst the user is using another. We map between the real
 * screen address and that used by DOSEMU here too.
 *
 * Attempts to use a cards own bios require the addition of the parameter
 * "graphics" to the video statement in "/etc/dosemu.conf". This will make
 * the emulator try to execute the card's initialization routine which is
 * normally located at address c000:0003. This can now be changed as an
 * option.
 *
 * DANG_END_MODULE
 *
 * vc.c - for the Linux DOS emulator
 *  Robert Sanders, gt8134b@prism.gatech.edu
 *
 * DANG_BEGIN_CHANGELOG
 *
 * $Date: 1995/02/25 21:54:25 $
 * $Source: /home/src/dosemu0.60/video/RCS/vc.c,v $
 * $Revision: 1.13 $
 * $State: Exp $
 *
 * Revision 1.3  1993/10/03  21:38:22  root
 * Checking the LegendClockSelect stuff ARGH !
 *
 * Revision 1.2  1993/09/30  21:45:24  root
 * Another extensive overhaul of ET4000 stuff
 *
 * Revision 1.1  1993/09/30  21:15:39  root
 * Initial revision
 *
 * Revision 1.5  1993/09/18  20:19:35  root
 * Added CRT 0x34, SEQ 0x06 and general alteration of ET4000 save/restore etc..
 *
 * Revision 1.4  1993/09/10  20:06:23  root
 * Increased Printing of ETX  Regs
 *
 * Revision 1.3  1993/09/06  22:24:35  root
 * Added et4000 regs to dump_video_regs
 *
 * Revision 1.2  1993/09/06  17:42:55  root
 * Added quick alpha switch via registers
 *
 * Revision 1.1  1993/09/06  17:20:38  root
 * Initial revision
 *
 * Revision 1.6  1993/09/05  15:32:13  root
 * *** empty log message ***
 *
 * Revision 1.5  1993/09/05  15:29:39  root
 * Added et4000 extended reg setting on bank read/write. Added some comments.
 *
 * Revision 1.4  1993/09/05  15:18:04  root
 * Banks are back in order, atleast for Trident :-), Why are they not
 * using page bit inverted as documented is beyond me....
 *
 * Revision 1.3  1993/09/05  12:30:54  root
 * Fixed protection violation (bad printf)
 *
 * Revision 1.2  1993/08/29  22:58:43  root
 * Bank modes are out of order
 *
 * Revision 1.1  1993/08/29  20:56:05  root
 * Initial revision
 *
 * Revision 1.2  1993/07/14  04:30:35  rsanders
 * changed some printf()s to v_printf()s.
 *
 * Revision 1.1  1993/07/07  00:49:06  root
 * Initial revision
 *
 * Revision 1.4  1993/05/20  05:19:48  root
 * added background text writing in mode 3
 *
 * Revision 1.3  1993/05/16  22:45:33  root
 * fixed int10h ah=9/ah=0xa problems.  also fixed screen page support.
 * still has some bugs with non-console video updating.
 *
 * Revision 1.2  1993/05/04  05:29:22  root
 * added console switching, new parse commands, and serial emulation
 *
 * Revision 1.1  1993/04/05  17:25:13  root
 * Initial revision
 *
 * DANG_END_CHANGELOG
 */


/* why ?? */
#define VGA_SAVE_PAGE0

#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
/*#include <termio.h>*/
#include <sys/time.h>
#ifndef USE_SLANG
#include <ncurses.h>		/*termcap.h*/
#endif
#ifdef WHY_DONT_PEOPLE_HAVE_THIS
#include <sys/mman.h>
#else
#define MAP_ANON MAP_ANONYMOUS
extern caddr_t mmap __P ((caddr_t __addr, size_t __len,
			  int __prot, int __flags, int __fd, off_t __off));
extern int munmap __P ((caddr_t __addr, size_t __len));
#include <linux/mman.h>
#endif
#include <signal.h>
#include <sys/stat.h>
#include <linux/vt.h>
#include <linux/kd.h>

#include "emu.h"
#include "memory.h"
#include "termio.h"
#include "mouse.h"
#include "dosio.h"
#include "machcompat.h"
#include "bios.h"
#include "port.h"

#include "video.h"
#include "vc.h"
#include "vga.h"
#include "s3.h"
#include "trident.h"
#include "et4000.h"

extern void child_close_mouse ();
extern void child_open_mouse ();
extern void clear_screen (int, int);
extern inline void console_update_cursor (int, int, int, int);
static void set_dos_video ();
void put_video_ram ();


extern int
  dosemu_sigaction (int sig, struct sigaction *, struct sigaction *);
static void SIGRELEASE_call (void);
static void SIGACQUIRE_call (void);

void dump_video_regs (void);
static void dump_video ();
void dump_video_linux ();
static u_char video_type = PLAINVGA;


static int  color_text;

#define MAXDOSINTS 032
#define MAXMODES 34


static void release_vt (int sig, struct sigcontext_struct context);
static void acquire_vt (int sig, struct sigcontext_struct context);
void get_video_ram (int);

#if 1
#undef SETSIG
#define SETSIG(sa, sig, fun)	sa.sa_handler = fun; \
				sa.sa_flags = 0; \
				sa.sa_mask = 0; \
				sigaction(sig, &sa, NULL);
#endif

static inline void
forbid_switch (void)
{
  scr_state.vt_allow = 0;
}

void
allow_switch (void)
{
  scr_state.vt_allow = 1;
  if (scr_state.vt_requested)
    {
      v_printf ("VID: clearing old vt request\n");
      SIGRELEASE_call ();
    }
}

static void
parent_close_mouse (void)
{
  if (mice->intdrv)
    {
      DOS_SYSCALL (close (mice->fd));
      if (use_sigio)
	{
	  FD_CLR (mice->fd, &fds_sigio);
	}
      else
	FD_CLR (mice->fd, &fds_no_sigio);
    }
  else
    child_close_mouse ();
}

static void
parent_open_mouse (void)
{
  if (mice->intdrv)
    {
      static int old_mice_flags;
      mice->fd = DOS_SYSCALL (open (mice->dev, O_RDWR | O_NONBLOCK));
      if (use_sigio)
	{
	  old_mice_flags = fcntl (mice->fd, F_GETFL);
	  fcntl (mice->fd, F_SETOWN, getpid ());
	  fcntl (mice->fd, F_SETFL, old_mice_flags | use_sigio);
	  FD_SET (mice->fd, &fds_sigio);
	}
      else
	FD_SET (mice->fd, &fds_no_sigio);
    }
  else
    child_open_mouse ();
}

static inline void
SIGACQUIRE_call (void)
{
  if (config.console_video)
    {
      get_video_ram (WAIT);
      set_dos_video ();
      /*      if (config.vga) dos_unpause(); */
    }
  parent_open_mouse ();
}

static void
acquire_vt (int sig, struct sigcontext_struct context)
{
  v_printf ("VID: Acquiring VC\n");
  forbid_switch ();
  if (ioctl (kbd_fd, VT_RELDISP, VT_ACKACQ))	/* switch acknowledged */
    v_printf ("VT_RELDISP failed (or was queued)!\n");
  allow_switch ();
  SIGNAL_save (SIGACQUIRE_call);
  scr_state.current = 1;
}

static inline void
get_permissions (void)
{
  DOS_SYSCALL (set_ioperm (0x3b0, 0x3df - 0x3b0 + 1, 1));
}

static inline void
giveup_permissions (void)
{
  DOS_SYSCALL (set_ioperm (0x3b0, 0x3df - 0x3b0 + 1, 0));
}

static void
set_dos_video (void)
{
  if (!config.vga)
    return;

  v_printf ("Setting DOS video: gfx_mode: %d\n", gfx_mode);

  /* jesx */
  /* After all that fun up there, get permissions and save/restore states */
  if (video_initialized)
    {
      v_printf ("Acquiring vt, restoring dosemu_regs\n");
      get_perm ();
#if 0
      dump_video ();
#endif
      restore_vga_state (&dosemu_regs);
    }

}

void
set_linux_video (void)
{
  if (!config.vga)
    return;

  /* jesx */
  /* After all that fun  rel perms and save/restore states */
  if (video_initialized)
    {
      v_printf ("Storing dosemu_regs, Releasing vt mode=%02x\n", *(u_char *) 0x449);
      dosemu_regs.video_mode = *(u_char *) 0x449;
      save_vga_state (&dosemu_regs);
#if 0
      dump_video ();
#endif
      if (linux_regs.mem != (u_char) NULL)
	{
	  v_printf ("Restoring linux_regs, Releasing vt\n");
#if 0
	  dump_video_linux ();
#endif
	  restore_vga_state (&linux_regs);
	}
      release_perm ();
    }
}

static void
SIGRELEASE_call (void)
{

  if (scr_state.current == 1)
    {
      v_printf ("VID: Releasing VC\n");
      parent_close_mouse ();
      if (!scr_state.vt_allow)
	{
	  v_printf ("disallowed vt switch!\n");
#if 0
	  scr_state.vt_requested = 1;
#endif
	  v_printf ("Released_vt finished!\n");
	  return;
	}

      if (config.console_video)
	{
	  unsigned short pos;

#if 0 /* 94/01/12 Why was this here :-( ? */

	  /* Read the cursor position from the 6845 registers. */
	  /* Must have bios_video_port initialized */
	  asm volatile ("movw %1,%%dx; movb $0xe,%%al; outb %%al,%%dx; "
			"incl %%edx; in %%dx,%%al; mov %%al,%%ah; "
			"decl %%edx; movb $0xf,%%al; outb %%al,%%dx; "
			"incl %%edx; inb %%dx,%%al; mov %%ax,%0"
			:"=g" (pos):"m" (READ_WORD(BIOS_VIDEO_PORT)):"%dx", "%ax");

	  /* Let kernel know the cursor location. */
	  console_update_cursor (pos % 80, pos / 80, 1, 1);
#endif

	  set_linux_video ();
	  put_video_ram ();

	  /*      if (config.vga) dos_pause(); */
	  scr_state.current = 0;
	}
    }

  scr_state.current = 0;	/* our console is no longer current */
  if (do_ioctl (kbd_fd, VT_RELDISP, 1))	/* switch ok by me */
    v_printf ("VT_RELDISP failed!\n");
}

int
wait_vc_active (void)
{
  if (ioctl (kbd_fd, VT_WAITACTIVE, scr_state.console_no) < 0)
    {
      error ("ERROR: VT_WAITACTIVE for %d gave %d: %s\n", scr_state.console_no,
	     errno, strerror (errno));
      return -1;
    }
  else
    return 0;
}

static inline void
release_vt (int sig, struct sigcontext_struct context)
{
  SIGNAL_save (SIGRELEASE_call);
}

/* allows remapping even if memory is mapped in...this is useful, as it
 * remembers where the video mem *was* mapped, unmaps from that, and then
 * remaps it to where the text page number says it should be
 */
void
get_video_ram (int waitflag)
{
  char *graph_mem;
  char *sbase;
  size_t ssize;

  char *textbuf = NULL, *vgabuf = NULL;

  v_printf ("get_video_ram STARTED\n");
  if (config.vga)
    {
      ssize = GRAPH_SIZE;
      sbase = (char *) GRAPH_BASE;
    }
  else
    {
      ssize = TEXT_SIZE;
      sbase = PAGE_ADDR (READ_BYTE(BIOS_CURRENT_SCREEN_PAGE));
    }

  if (waitflag == WAIT)
    {
#if 0
      config.console_video = 0;
#endif
      v_printf ("VID: get_video_ram WAITING\n");
      /* XXX - wait until our console is current (mixed signal functions) */
      do
	{
	  if (!wait_vc_active ())
	    break;
	  v_printf ("Keeps waiting...And\n");
	}
      while (errno == EINTR);
    }

  if (config.vga)
    {
      if (READ_BYTE(BIOS_VIDEO_MODE) == 3 && READ_BYTE(BIOS_CURRENT_SCREEN_PAGE) < 8)
	{
	  textbuf = malloc (TEXT_SIZE * 8);
	  memcpy (textbuf, PAGE_ADDR (0), TEXT_SIZE * 8);
	}

      if (scr_state.mapped)
	{
	  vgabuf = malloc (GRAPH_SIZE);
	  memcpy (vgabuf, (caddr_t) GRAPH_BASE, GRAPH_SIZE);
	  graph_mem = (char *) mmap ((caddr_t) GRAPH_BASE,
				     GRAPH_SIZE,
				     PROT_EXEC | PROT_READ | PROT_WRITE,
				     MAP_PRIVATE | MAP_FIXED | MAP_ANON,
				     -1,
				     0);
	  memcpy ((caddr_t) GRAPH_BASE, vgabuf, GRAPH_SIZE);
	}
    }
  else
    {
      textbuf = malloc (TEXT_SIZE);
      memcpy (textbuf, scr_state.virt_address, TEXT_SIZE);

      if (scr_state.mapped)
	{
	  graph_mem = (char *) mmap ((caddr_t) scr_state.virt_address,
				     TEXT_SIZE,
				     PROT_EXEC | PROT_READ | PROT_WRITE,
				     MAP_PRIVATE | MAP_FIXED | MAP_ANON,
				     -1,
				     0);
	  memcpy (scr_state.virt_address, textbuf, TEXT_SIZE);
	}
    }
  scr_state.mapped = 0;

  if (config.vga)
    {
      if (READ_BYTE(BIOS_VIDEO_MODE) == 3)
	{
	  if (dosemu_regs.mem && textbuf)
	    memcpy (dosemu_regs.mem, textbuf, dosemu_regs.save_mem_size[0]);
	  /*      else error("ERROR: no dosemu_regs.mem!\n"); */
	}
      g_printf ("mapping GRAPH_BASE\n");
      open_kmem ();
      graph_mem = (char *) mmap ((caddr_t) GRAPH_BASE,
				 (size_t) (GRAPH_SIZE),
				 PROT_READ | PROT_WRITE,
				 MAP_SHARED | MAP_FIXED,
				 mem_fd,
				 GRAPH_BASE);

      close_kmem ();
      /* the code below is done by the video save/restore code */
      get_permissions ();
    }
  else
    {
      /* this is used for page switching */
      if (PAGE_ADDR (READ_BYTE(BIOS_CURRENT_SCREEN_PAGE)) != scr_state.virt_address)
	memcpy (textbuf, PAGE_ADDR (READ_BYTE(BIOS_CURRENT_SCREEN_PAGE)), TEXT_SIZE);

      g_printf ("mapping PAGE_ADDR\n");

      open_kmem ();
#if 0
      /* Map CGA, etc text memory to HGA memory.
       Useful for debugging systems with HGA or MDA cards.
     */
      graph_mem = (char *) mmap ((caddr_t) 0xb8000,
				 TEXT_SIZE,
				 PROT_READ | PROT_WRITE,
				 MAP_SHARED | MAP_FIXED,
				 mem_fd,
				 phys_text_base);
#endif	/* 0 */

      graph_mem = (char *) mmap ((caddr_t) PAGE_ADDR (READ_BYTE(BIOS_CURRENT_SCREEN_PAGE)),
				 TEXT_SIZE,
				 PROT_READ | PROT_WRITE,
				 MAP_SHARED | MAP_FIXED,
				 mem_fd,
				 phys_text_base);
      close_kmem ();

      if ((long) graph_mem < 0)
	{
	  error ("ERROR: mmap error in get_video_ram (text)\n");
	  return;
	}
      else
	v_printf ("CONSOLE VIDEO address: %p %p %p\n", (void *) graph_mem,
		  (void *) phys_text_base, (void *) PAGE_ADDR (READ_BYTE(BIOS_CURRENT_SCREEN_PAGE)));

      get_permissions ();
      /* copy contents of page onto video RAM */
      memcpy ((caddr_t) PAGE_ADDR (READ_BYTE(BIOS_CURRENT_SCREEN_PAGE)), textbuf, TEXT_SIZE);
    }

  if (vgabuf)
    free (vgabuf);
  if (textbuf)
    free (textbuf);

  scr_state.pageno = READ_BYTE(BIOS_CURRENT_SCREEN_PAGE);
  scr_state.virt_address = PAGE_ADDR (READ_BYTE(BIOS_CURRENT_SCREEN_PAGE));
  scr_state.phys_address = graph_mem;
  scr_state.mapped = 1;
}


void
put_video_ram (void)
{
  char *putbuf = (char *) malloc (TEXT_SIZE);
  char *graph_mem;

  if (scr_state.mapped)
    {
      v_printf ("put_video_ram called\n");

      if (config.vga)
	{
	  open_kmem();
	  graph_mem = (char *) mmap ((caddr_t) GRAPH_BASE,
				     GRAPH_SIZE,
				     PROT_EXEC | PROT_READ | PROT_WRITE,
				     MAP_PRIVATE | MAP_FIXED | MAP_ANON,
				     -1,
				     0);
	  close_kmem();
	  if (dosemu_regs.mem && READ_BYTE(BIOS_VIDEO_MODE) == 3 && READ_BYTE(BIOS_CURRENT_SCREEN_PAGE) < 8) {
	    memcpy ((caddr_t) PAGE_ADDR(0), dosemu_regs.mem, dosemu_regs.save_mem_size[0]);
	  }
	}
      else
	{
	  memcpy (putbuf, scr_state.virt_address, TEXT_SIZE);
	  graph_mem = (char *) mmap ((caddr_t) scr_state.virt_address,
				     TEXT_SIZE,
				     PROT_EXEC | PROT_READ | PROT_WRITE,
				     MAP_PRIVATE | MAP_FIXED | MAP_ANON,
				     -1,
				     0);
	  memcpy (scr_state.virt_address, putbuf, TEXT_SIZE);
	}

      giveup_permissions ();
      scr_state.mapped = 0;
    }
  else
    warn ("VID: put_video-ram but not mapped!\n");


  if (putbuf)
    free (putbuf);
  v_printf ("put_video_ram completed\n");
}

/* this puts the VC under process control */
void
set_process_control (void)
{
  struct vt_mode vt_mode;
  struct sigaction sa;

  vt_mode.mode = VT_PROCESS;
  vt_mode.waitv = 0;
  vt_mode.relsig = SIG_RELEASE;
  vt_mode.acqsig = SIG_ACQUIRE;
  vt_mode.frsig = 0;

  scr_state.vt_requested = 0;	/* a switch has not been attempted yet */
  allow_switch ();

  NEWSETQSIG (SIG_RELEASE, release_vt);
  NEWSETQSIG (SIG_ACQUIRE, acquire_vt);

  if (do_ioctl (kbd_fd, VT_SETMODE, (int) &vt_mode))
    v_printf ("initial VT_SETMODE failed!\n");
  v_printf ("VID: Set process control\n");
}

void
clear_process_control (void)
{
  struct vt_mode vt_mode;

  vt_mode.mode = VT_AUTO;
  ioctl (kbd_fd, VT_SETMODE, (int) &vt_mode);
  signal (SIG_RELEASE, SIG_IGN);
  signal (SIG_ACQUIRE, SIG_IGN);
}


/* why count ??? */
static u_char kmem_open_count = 0;

void
open_kmem (void)
{
  /* as I understad it, /dev/kmem is the kernel's view of memory,
     * and /dev/mem is the identity-mapped (i.e. physical addressed)
     * memory. Currently under Linux, both are the same.
     */

  kmem_open_count++;

  if (mem_fd != -1)
    return;
  mem_fd = open("/dev/kmem", O_RDWR);
  if (mem_fd < 0)
    {
      error ("ERROR: can't open /dev/kmem: errno=%d, %s \n",
	     errno, strerror (errno));
      leavedos (0);
      return;
    }
  g_printf ("Kmem opened successfully\n");
}

void
close_kmem (void)
{

  if (kmem_open_count)
    {
      kmem_open_count--;
      if (kmem_open_count)
	return;
      close (mem_fd);
      mem_fd = -1;
      v_printf ("Kmem closed successfully\n");
    }
}

int
vc_active (void)
{				/* return 1 if our VC is active */
  struct vt_stat vtstat;

  g_printf ("VC_ACTIVE!\n");
  ioctl (kbd_fd, VT_GETSTATE, &vtstat);
  g_printf ("VC_ACTIVE: ours: %d, active: %d\n", scr_state.console_no, vtstat.v_active);
  return ((vtstat.v_active == scr_state.console_no));
}

void
set_vc_screen_page (int page)
{
  WRITE_BYTE(BIOS_CURRENT_SCREEN_PAGE, page);

  /* okay, if we have the current console, and video ram is mapped.
   * this has to be "atomic," or there is a "race condition": the
   * user may change consoles between our check and our remapping
   */
  forbid_switch ();
  if (scr_state.mapped)		/* should I check for foreground VC instead? */
    get_video_ram (WAIT);
  allow_switch ();
}


/* get ioperms to allow havoc to occur */
int
get_perm (void)
{
  permissions++;
  if (permissions > 1)
    {
      return 0;
    }
  if (config.vga)
    {				/* hope this will not lead to problems with ega/cga */
      /* get I/O permissions for VGA registers */
      if (ioperm (0x3b0, 0x3df - 0x3b0 + 1, 1))
	{
	  v_printf ("VGA: can't get I/O permissions \n");
	  exit (-1);
	}
      if (config.chipset == S3 && (ioperm (0x102, 1, 1) || ioperm (0x2ea, 4, 1)))
	{
	  v_printf ("S3: can't get I/O permissions \n");
	  exit (-1);
	}
      /* color or monochrome text emulation? */
      color_text = port_in (MIS_R) & 0x01;

      /* chose registers for color/monochrome emulation */
      if (color_text)
	{
	  CRT_I = CRT_IC;
	  CRT_D = CRT_DC;
	  IS1_R = IS1_RC;
	  FCR_W = FCR_WC;
	}
      else
	{
	  CRT_I = CRT_IM;
	  CRT_D = CRT_DM;
	  IS1_R = IS1_RM;
	  FCR_W = FCR_WM;
	}
    }
  else if (config.usesX)
    {
      if (ioperm (0x3b4, 1, 1) ||
	  ioperm (0x3b5, 1, 1) ||
	  ioperm (0x3b8, 1, 1) ||
	  ioperm (0x3ba, 1, 1) ||
	  ioperm (0x3bf, 1, 1))
	{
	  v_printf ("HGC: can't get I/O permissions \n");
	  exit (-1);
	}
    }
  v_printf ("Permission allowed\n");
  return 0;
}

/* Stop io to card */
int
release_perm (void)
{
  if (permissions > 0)
    {
      permissions--;
      if (permissions > 0)
	{
	  return 0;
	}
      if (config.vga)
	{			/* hope this will not lead to problems with ega/cga */
	  /* get I/O permissions for VGA registers */
	  /* release I/O permissions for VGA registers */
	  if (ioperm (0x3b0, 0x3df - 0x3b0 + 1, 0))
	    {
	      v_printf ("VGA: can't release I/O permissions \n");
	      leavedos (-1);
	    }
	  if (config.chipset == S3 && (ioperm (0x102, 1, 0) || ioperm (0x2ea, 4, 0)))
	    {
	      v_printf ("S3: can't release I/O permissions\n");
	      leavedos (-1);
	    }
	}
      else if (config.usesX)
	{
	  if (ioperm (0x3b4, 1, 0) ||
	      ioperm (0x3b5, 1, 0) ||
	      ioperm (0x3b8, 1, 0) ||
	      ioperm (0x3ba, 1, 0) ||
	      ioperm (0x3bf, 1, 0))
	    {
	      v_printf ("HGC: can't release I/O permissions \n");
	      exit (-1);
	    }
	}
      v_printf ("Permission disallowed\n");
    }
  else
    v_printf ("Permissions already at 0\n");
  return 0;

}

int
set_regs (u_char regs[])
{
  int i;

  /* port_out(dosemu_regs.regs[FCR], FCR_W); */
  /* update misc output register */
  port_out (regs[MIS], MIS_W);

  /* write sequencer registers */
  /* synchronous reset on */
  port_out (0x00, SEQ_I);
  port_out (0x01, SEQ_D);
  port_out (0x01, SEQ_I);
  port_out (regs[SEQ + 1] | 0x20, SEQ_D);
  /* synchronous reset off */
  port_out (0x00, SEQ_I);
  port_out (0x03, SEQ_D);
  for (i = 2; i < SEQ_C; i++)
    {
      port_out (i, SEQ_I);
      port_out (regs[SEQ + i], SEQ_D);
    }

  /* deprotect CRT registers 0-7 */
  port_out (0x11, CRT_I);
  port_out (port_in (CRT_D) & 0x7F, CRT_D);

  /* write CRT registers */
  for (i = 0; i < CRT_C; i++)
    {
      port_out (i, CRT_I);
      port_out (regs[CRT + i], CRT_D);
    }

  /* write graphics controller registers */
  for (i = 0; i < GRA_C; i++)
    {
      port_out (i, GRA_I);
      port_out (regs[GRA + i], GRA_D);
    }

  /* write attribute controller registers */
  for (i = 0; i < ATT_C; i++)
    {
      port_in (IS1_R);		/* reset flip-flop */
      port_out (i, ATT_IW);
      port_out (regs[ATT + i], ATT_IW);
    }

  /* port_out(dosemu_regs.regs[CRTI], CRT_I);
port_out(dosemu_regs.regs[GRAI], GRA_I);
port_out(dosemu_regs.regs[SEQI], SEQ_I); */
  /* v_printf("CRTI=0x%02x\n",dosemu_regs.regs[CRTI]);
v_printf("GRAI=0x%02x\n",dosemu_regs.regs[GRAI]);
v_printf("SEQI=0x%02x\n",dosemu_regs.regs[SEQI]); */
  return (0);
}


/* Reset Attribute registers */
static inline void
reset_att (void)
{
  port_in (IS1_R);
}


/* Attempt to virtualize calls to video ports */

u_char att_d_index = 0;
static isr_read = 0;

u_char
video_port_in (int port)
{
  /* v_printf("Video read on port 0x%04x.\n",port); */
  switch (port)
    {
    case CRT_IC:
    case CRT_IM:
      v_printf ("Read Index CRTC 0x%02x\n", dosemu_regs.regs[CRTI]);
      return (dosemu_regs.regs[CRTI]);
    case CRT_DC:
    case CRT_DM:
      if (dosemu_regs.regs[CRTI] <= 0x18)
	{
	  v_printf ("Read Data at CRTC Index 0x%02x = 0x%02x \n", dosemu_regs.regs[CRTI], dosemu_regs.regs[CRT + dosemu_regs.regs[CRTI]]);
	  return (dosemu_regs.regs[CRT + dosemu_regs.regs[CRTI]]);
	}
      else
	return (ext_video_port_in (port));
      break;
    case GRA_I:
      v_printf ("Read Index GRAI 0x%02x\n", GRAI);
      return (dosemu_regs.regs[GRAI]);
    case GRA_D:
      if (dosemu_regs.regs[GRAI] < 0x0a)
	{
	  v_printf ("Read Data at GRA  Index 0x%02x = 0x%02x \n", dosemu_regs.regs[GRAI], dosemu_regs.regs[GRA + dosemu_regs.regs[GRAI]]);
	  return (dosemu_regs.regs[GRA + dosemu_regs.regs[GRAI]]);
	}
      else
	return (ext_video_port_in (port));
      break;
    case SEQ_I:
      v_printf ("Read Index SEQI 0x%02x\n", SEQI);
      return (dosemu_regs.regs[SEQI]);
    case SEQ_D:
      if (dosemu_regs.regs[SEQI] < 0x07)
	{
	  v_printf ("Read Data at SEQ Index 0x%02x = 0x%02x \n", dosemu_regs.regs[SEQI], dosemu_regs.regs[SEQ + dosemu_regs.regs[SEQI]]);
	  return (dosemu_regs.regs[SEQ + dosemu_regs.regs[SEQI]]);
	}
      else
	return (ext_video_port_in (port));
      break;
    case ATT_IW:
      v_printf ("Read Index ATTIW 0x%02x\n", att_d_index);
      return (att_d_index);
    case ATT_R:
      if (att_d_index > 20)
	return (ext_video_port_in (port));
      else
	{
	  v_printf ("Read Index ATTR 0x%02x\n", dosemu_regs.regs[ATT + att_d_index]);
	  return (dosemu_regs.regs[ATT + att_d_index]);
	}
      break;
    case IS1_RC:
    case IS1_RM:
      v_printf ("Read ISR1_R 0x%02x\n", dosemu_regs.regs[ISR1]);
      isr_read = 1;		/* Next ATT write will be to the index */
      return (dosemu_regs.regs[ISR1]);
    case MIS_R:
      v_printf ("Read MIS_R 0x%02x\n", dosemu_regs.regs[MIS]);
      return (dosemu_regs.regs[MIS]);
    case IS0_R:
      v_printf ("Read ISR0_R 0x%02x\n", dosemu_regs.regs[ISR0]);
      return (dosemu_regs.regs[ISR0]);
    case PEL_IW:
      v_printf ("Read PELIW 0x%02x\n", dosemu_regs.regs[PELIW] / 3);
      return (dosemu_regs.regs[PELIW] / 3);
    case PEL_IR:
      v_printf ("Read PELIR 0x%02x\n", dosemu_regs.regs[PELIR] / 3);
      return (dosemu_regs.regs[PELIR] / 3);
    case PEL_D:
      v_printf ("Read PELIR Data 0x%02x\n", dosemu_regs.pal[dosemu_regs.regs[PELIR]]);
      return (dosemu_regs.pal[dosemu_regs.regs[PELIR]++]);
    case PEL_M:
      v_printf ("Read PELM  Data 0x%02x\n", dosemu_regs.regs[PELM] == 0 ? 0xff : dosemu_regs.regs[PELM]);
      return (dosemu_regs.regs[PELM] == 0 ? 0xff : dosemu_regs.regs[PELM]);
    default:
      return (ext_video_port_in (port));
    }
}

void
video_port_out (u_char value, int port)
{
  /* v_printf("Video write on port 0x%04x,byte 0x%04x.\n",port, value); */
  switch (port)
    {
    case CRT_IC:
    case CRT_IM:
      v_printf ("Set Index CRTC 0x%02x\n", value);
      dosemu_regs.regs[CRTI] = value;
      break;
    case CRT_DC:
    case CRT_DM:
      if (dosemu_regs.regs[CRTI] <= 0x18)
	{
	  v_printf ("Write Data at CRTC Index 0x%02x = 0x%02x \n", dosemu_regs.regs[CRTI], value);
	  dosemu_regs.regs[CRT + dosemu_regs.regs[CRTI]] = value;
	}
      else
	ext_video_port_out (value, port);
      break;
    case GRA_I:
      v_printf ("Set Index GRAI 0x%02x\n", value);
      dosemu_regs.regs[GRAI] = value;
      break;
    case GRA_D:
      if (dosemu_regs.regs[GRAI] < 0x0a)
	{
	  v_printf ("Write Data at GRAI Index 0x%02x = 0x%02x \n", dosemu_regs.regs[GRAI], value);
	  dosemu_regs.regs[GRA + dosemu_regs.regs[GRAI]] = value;
	}
      else
	ext_video_port_out (value, port);
      break;
    case SEQ_I:
      v_printf ("Set Index SEQI 0x%02x\n", value);
      dosemu_regs.regs[SEQI] = value;
      break;
    case SEQ_D:
      if (dosemu_regs.regs[SEQI] <= 0x05)
	{
	  v_printf ("Write Data at SEQ Index 0x%02x = 0x%02x \n", dosemu_regs.regs[SEQI], value);
	  dosemu_regs.regs[SEQ + dosemu_regs.regs[SEQI]] = value;
	}
      else
	ext_video_port_out (value, port);
      break;
    case ATT_IW:
      if (isr_read)
	{
	  v_printf ("Set Index ATTI 0x%02x\n", value & 0x1f);
	  att_d_index = value & 0x1f;
	  isr_read = 0;
	}
      else
	{
	  isr_read = 1;
	  if (att_d_index > 20)
	    ext_video_port_out (value, port);
	  else
	    {
	      v_printf ("Write Data at ATT Index 0x%02x = 0x%02x \n", att_d_index, value);
	      dosemu_regs.regs[ATT + att_d_index] = value;
	    }
	}
      break;
    case MIS_W:
      v_printf ("Set MISW 0x%02x\n", value);
      dosemu_regs.regs[MIS] = value;
      break;
    case PEL_IW:
      v_printf ("Set PELIW  to 0x%02x\n", value);
      dosemu_regs.regs[PELIW] = value * 3;
      break;
    case PEL_IR:
      v_printf ("Set PELIR to 0x%02x\n", value);
      dosemu_regs.regs[PELIR] = value * 3;
      break;
    case PEL_D:
      v_printf ("Put PEL_D[0x%02x] = 0x%02x\n", dosemu_regs.regs[PELIW], value);
      dosemu_regs.pal[dosemu_regs.regs[PELIW]] = value;
      dosemu_regs.regs[PELIW] += 1;
      break;
    case PEL_M:
      v_printf ("Set PEL_M to 0x%02x\n", value);
      dosemu_regs.regs[PELM] = value;
      break;
    case GR1_P:
      v_printf ("Set GR1_P to 0x%02x\n", value);
      dosemu_regs.regs[GR1P] = value;
      break;
    case GR2_P:
      v_printf ("Set GR2_P to 0x%02x\n", value);
      dosemu_regs.regs[GR2P] = value;
      break;
    default:
      ext_video_port_out (value, port);
    }
  return;
}



/* Generally I will request a video mode change switch quickly, and have the
   actuall registers printed out,as well as the dump of what dump_video
   received and fix the differences */

/* Dump what's in the dosemu_regs */
static void
dump_video (void)
{
  u_short i;

  v_printf ("/* BIOS mode 0x%02X */\n", dosemu_regs.video_mode);
  v_printf ("static char regs[60] = {\n  ");
  for (i = 0; i < 12; i++)
    v_printf ("0x%02X,", dosemu_regs.regs[CRT + i]);
  v_printf ("\n  ");
  for (i = 12; i < CRT_C; i++)
    v_printf ("0x%02X,", dosemu_regs.regs[CRT + i]);
  v_printf ("\n  ");
  for (i = 0; i < 12; i++)
    v_printf ("0x%02X,", dosemu_regs.regs[ATT + i]);
  v_printf ("\n  ");
  for (i = 12; i < ATT_C; i++)
    v_printf ("0x%02X,", dosemu_regs.regs[ATT + i]);
  v_printf ("\n  ");
  for (i = 0; i < GRA_C; i++)
    v_printf ("0x%02X,", dosemu_regs.regs[GRA + i]);
  v_printf ("\n  ");
  for (i = 0; i < SEQ_C; i++)
    v_printf ("0x%02X,", dosemu_regs.regs[SEQ + i]);
  v_printf ("\n  ");
  v_printf ("0x%02X", dosemu_regs.regs[MIS]);
  v_printf ("\n};\n");
  v_printf ("Extended Regs/if applicable:\n");
  for (i = 0; i < MAX_X_REGS; i++)
    v_printf ("0x%02X,", dosemu_regs.xregs[i]);
  v_printf ("0x%02X\n", dosemu_regs.xregs[MAX_X_REGS]);
  v_printf ("Extended 16 bit Regs/if applicable:\n");
  for (i = 0; i < MAX_X_REGS16; i++)
    v_printf ("0x%04X,", dosemu_regs.xregs16[i]);
  v_printf ("0x%04X\n", dosemu_regs.xregs16[MAX_X_REGS16]);
}

/* Dump what's in the linux_regs */
void
dump_video_linux (void)
{
  u_short i;

  v_printf ("/* BIOS mode 0x%02X */\n", linux_regs.video_mode);
  v_printf ("static char regs[60] = {\n  ");
  for (i = 0; i < 12; i++)
    v_printf ("0x%02X,", linux_regs.regs[CRT + i]);
  v_printf ("\n  ");
  for (i = 12; i < CRT_C; i++)
    v_printf ("0x%02X,", linux_regs.regs[CRT + i]);
  v_printf ("\n  ");
  for (i = 0; i < 12; i++)
    v_printf ("0x%02X,", linux_regs.regs[ATT + i]);
  v_printf ("\n  ");
  for (i = 12; i < ATT_C; i++)
    v_printf ("0x%02X,", linux_regs.regs[ATT + i]);
  v_printf ("\n  ");
  for (i = 0; i < GRA_C; i++)
    v_printf ("0x%02X,", linux_regs.regs[GRA + i]);
  v_printf ("\n  ");
  for (i = 0; i < SEQ_C; i++)
    v_printf ("0x%02X,", linux_regs.regs[SEQ + i]);
  v_printf ("\n  ");
  v_printf ("0x%02X", linux_regs.regs[MIS]);
  v_printf ("\n};\n");
  v_printf ("Extended Regs/if applicable:\n");
  for (i = 0; i < MAX_X_REGS; i++)
    v_printf ("0x%02X,", linux_regs.xregs[i]);
  v_printf ("0x%02X\n", linux_regs.xregs[MAX_X_REGS]);
  v_printf ("Extended 16 bit Regs/if applicable:\n");
  for (i = 0; i < MAX_X_REGS16; i++)
    v_printf ("0x%04X,", linux_regs.xregs16[i]);
  v_printf ("0x%04X\n", linux_regs.xregs16[MAX_X_REGS16]);
}

/*
 * install_int_10_handler - install a handler for the video-interrupt (int 10)
 *                          at address INT10_SEG:INT10_OFFS. Currently
 *                          it's f100:4100.
 *                          The new handler is only installed, if the bios
 *                          handler at f100:4100 is not the appropriate on
 *                          that means, if we use not mda with X
 *                          The new handler is only installed, if the bios
 *                          handler at f100:4100 is not the appropriate on
 *                          that means, if we use not mda with X
 */

void 
install_int_10_handler (void)
{
  unsigned char *ptr;

#if 0
  if (!config.usesX)
    {
      /* Wrapper around call to video init c000:0003 */
      ptr = (u_char *) INT10_ADD;

      *ptr++ = 0x50;		/* push ax          */
      *ptr++ = 0xfb;		/* start interrupts STI  */

      *ptr++ = 0xb0;
      *ptr++ = 0x08;		/* mov  al,08  ; Start Video init     */

      *ptr++ = 0xcd;
      *ptr++ = 0xe6;		/* int  e6          */

      if (config.vbios_seg == 0xe000)
	{
	  *ptr++ = 0x9a;
	  *ptr++ = 0x03;
	  *ptr++ = 0x00;
	  *ptr++ = 0x00;
	  *ptr++ = 0xe0;	/* call e000:0003   */
	}
      else
	{
	  *ptr++ = 0x9a;
	  *ptr++ = 0x03;
	  *ptr++ = 0x00;
	  *ptr++ = 0x00;
	  *ptr++ = 0xc0;	/* call c000:0003   */
	}
      *ptr++ = 0xb0;
      *ptr++ = 0x09;		/* move al,09   ; Finished video init    */

      *ptr++ = 0xcd;
      *ptr++ = 0xe6;		/* int  e6          */

      *ptr++ = 0x58;		/* pop ax           */
      *ptr++ = 0xfb;		/* start interrupts STI  */
      *ptr++ = 0xcb;		/* retf             */
    }
#else
  if (config.vbios_seg == 0xe000) {
    extern void bios_f000_int10ptr(), bios_f000();
    ptr = (u_char *)((BIOSSEG << 4) + ((long)bios_f000_int10ptr - (long)bios_f000));
    *((long *)ptr) = 0xe0000003;
  }
#endif
}
