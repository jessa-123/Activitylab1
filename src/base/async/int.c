/* 
 * (C) Copyright 1992, ..., 2000 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */


#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/times.h>
#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

#include "config.h"
#include "emu.h"
#include "memory.h"
#include "timers.h"
#include "mouse.h"
#include "disks.h"
#include "bios.h"
#include "iodev.h"
#include "bitops.h"
#include "xms.h"
#include "int.h"
#include "dos2linux.h"
#include "video.h"
#include "vc.h"
#include "priv.h"
#include "doshelpers.h"

#ifdef USING_NET
#include "ipx.h"
#endif

#include "dpmi.h"

#include "keyb_server.h"

#define DJGPP_HACK	/* AV Feb 97 */
#undef  DEBUG_INT1A

#if X_GRAPHICS
/* prototype is in X.h -- 1998/03/08 sw */
int X_change_config(unsigned, void *);
#endif

extern void pci_bios(void);

/*
   This flag will be set when doing video routines so that special
   access can be given
*/
static u_char         in_video = 0;
static u_char         save_hi_ints[128];

static int            card_init = 0;
static unsigned long  precard_eip, precard_cs;

static struct timeval scr_tv;        /* For translating UNIX <-> DOS times */

/* set if some directories are mounted during startup */
int redir_state = 0;

#ifdef USE_MRP_JOYSTICK
#include <linux/joystick.h>
#include <fcntl.h>
#include <string.h>
static void mrp_read_joystick(void);
#endif

void kill_time(long usecs) {
   hitimer_t t_start;
   long t_dif;
   scr_tv.tv_sec = 0L;
   scr_tv.tv_usec = usecs;

   t_start = GETusTIME(0);
   while ((int) select(STDIN_FILENO, NULL, NULL, NULL, &scr_tv) < (int) 1)
     {
	t_dif = (long)(GETusTIME(0)-t_start);

        if ((t_dif >= usecs) || (errno != EINTR))
          return ;
        scr_tv.tv_sec = 0L;
        scr_tv.tv_usec = usecs - t_dif;
     }
}

/*
 * DANG_BEGIN_FUNCTION DEFAULT_INTERRUPT 
 *
 * description:
 * DEFAULT_INTERRUPT is the default interrupt service routine 
 * called when DOSEMU initializes.
 *
 * DANG_END_FUNCTION
 */

static void default_interrupt(u_char i) {
  di_printf("int 0x%02x, ax=0x%04x\n", i, LWORD(eax));

  if (!IS_REDIRECTED(i) ||
#ifndef USE_NEW_INT
      (LWORD(cs) == BIOSSEG && LWORD(eip) == (i * 16 + 2))) {
#else /* USE_NEW_INT */
      ((SEGOFF2LINEAR(BIOSSEG, INT_OFF(i)) +2) == SEGOFF2LINEAR(_CS, _IP))) {
#endif /* USE_NEW_INT */
    g_printf("DEFIVEC: int 0x%02x @ 0x%04x:0x%04x\n", i, ISEG(i), IOFF(i));

    /* This is here for old SIGILL's that modify IP */
    if (i == 0x00)
      LWORD(eip)+=2;
  } else if (IS_IRET(i)) {
    if ((i != 0x2a) && (i != 0x28))
      g_printf("just an iret 0x%02x\n", i);
  } else
    real_run_int(i);
}

static void process_master_boot_record(void)
{
  /* Ok, _we_ do the MBR code in 32-bit C code,
   * so this obviously is _not_ stolen from any DOS code ;-)
   *
   * Now, what _does_ the original MSDOS MBR?
   * 1. It sets DS,ES,SS to zero
   * 2. It sets the stack pinter to just below the loaded MBR (SP=0x7c00)
   * 3. It moves itself down to 0:0x600
   * 4. It searches for a partition having the bootflag set (=0x80)
   * 5. It loads the bootsector of this partition to 0:0x7c00
   * 6. It does a long jump to 0:0x7c00, with following registers set:
   *    DS,ES,SS = 0
   *    BP,SI pointing to the partition entry within 0:600 MBR
   *    DI = 0x7dfe
   */
   struct partition {
     unsigned char bootflag;
     unsigned char start_head;
     unsigned char start_sector;
     unsigned char start_track;
     unsigned char OS_type;
     unsigned char end_head;
     unsigned char end_sector;
     unsigned char end_track;
     unsigned long num_sect_preceding;
     unsigned long num_sectors;
   } __attribute__((packed));
   struct mbr {
     char code[0x1be];
     struct partition partition[4];
     unsigned short bootmagic;
   } __attribute__((packed));
   struct mbr *mbr = (struct mbr *)0x600;
   struct mbr *bootrec = (struct mbr *)0x7c00;
   int i;
      
   memcpy(mbr, bootrec, 0x200);	/* move the mbr down */

   for (i=0; i<4; i++) {
     if (mbr->partition[i].bootflag == 0x80) break;
   }
   if (i >=4) {
     /* aiee... no bootflags sets */
     p_dos_str("\n\rno bootflag set, Leaving DOS...\n\r");
     leavedos(99);
   }
   LWORD(cs) = LWORD(ds) = LWORD(es) = LWORD(ss) =0;
   LWORD(esp) = 0x7c00;
   LO(dx) = 0x80;  /* drive C:, DOS boots only from C: */
   HI(dx) = mbr->partition[i].start_head;
   LO(cx) = mbr->partition[i].start_sector;
   HI(cx) = mbr->partition[i].start_track;
   LWORD(eax) = 0x0201;  /* read one sector */
   LWORD(ebx) = 0x7c00;  /* target offset, ES is 0 */
   int13(13); /* we simply call our INT13 routine, hence we will not have
                 to worry about future changements to this code */
   if ((REG(eflags) & CF) || (bootrec->bootmagic != 0xaa55)) {
     /* error while booting */
     p_dos_str("\n\rerror on reading bootsector, Leaving DOS...\n\r");
     leavedos(99);
   }
   LWORD(edi)= 0x7dfe;
   LWORD(eip) = 0x7c00;
   LWORD(ebp) = LWORD(esi) = (unsigned)&mbr->partition[i];
}

/* returns 1 if dos_helper() handles it, 0 otherwise */
static int dos_helper(void)
{
#ifdef X86_EMULATOR
  extern void enter_cpu_emu(void);
  extern void leave_cpu_emu(void);
#endif

  switch (LO(ax)) {
  case DOS_HELPER_DOSEMU_CHECK:			/* Linux dosemu installation test */
    LWORD(eax) = 0xaa55;
    LWORD(ebx) = VERSION * 0x100 + SUBLEVEL; /* major version 0.49 -> 0049 */
    /* The patch level in the form n.n is a float...
     * ...we let GCC at compiletime translate it to 0xHHLL, HH=major, LL=minor.
     * This way we avoid usage of float instructions.
     */
    LWORD(ecx) = ((((int)(PATCHLEVEL * 100))/100) << 8) + (((int)(PATCHLEVEL * 100))%100);
    LWORD(edx) = (config.X)? 0x1:0;  /* Return if running under X */
    g_printf("WARNING: dosemu installation check\n");
    show_regs(__FILE__, __LINE__);
    break;

  case DOS_HELPER_SHOW_REGS:     /* SHOW_REGS */
    show_regs(__FILE__, __LINE__);
    break;

  case DOS_HELPER_SHOW_INTS:	/* SHOW INTS, BH-BL */
    show_ints(HI(bx), LO(bx));
    break;

  case DOS_HELPER_PRINT_STRING:	/* PRINT STRING ES:DX */
    g_printf("DOS likes us to print a string\n");
    ds_printf("DOS to EMU: \"%s\"\n",SEG_ADR((char *), es, dx));
    break;



  case DOS_HELPER_ADJUST_IOPERMS:  /* SET IOPERMS: bx=start, cx=range,
				   carry set for get, clear for release */
  {
    int cflag = LWORD(eflags) & CF ? 1 : 0;

    i_printf("I/O perms: 0x%04x 0x%04x %d\n", LWORD(ebx), LWORD(ecx), cflag);
    if (set_ioperm(LWORD(ebx), LWORD(ecx), cflag)) {
      error("SET_IOPERMS request failed!!\n");
      CARRY;			/* failure */
    } else {
      if (cflag)
	warn("WARNING! DOS can now access I/O ports 0x%04x to 0x%04x\n",
	     LWORD(ebx), LWORD(ebx) + LWORD(ecx) - 1);
      else
	warn("Access to ports 0x%04x to 0x%04x clear\n",
	     LWORD(ebx), LWORD(ebx) + LWORD(ecx) - 1);
      NOCARRY;		/* success */
    }
  }
  break;

  case DOS_HELPER_CONTROL_VIDEO:	/* initialize video card */
    if (LO(bx) == 0) {
      if (set_ioperm(0x3b0, 0x3db - 0x3b0, 0))
	warn("couldn't shut off ioperms\n");
#ifndef USE_NEW_INT
      SETIVEC(0x10, BIOSSEG, 0x10 * 0x10);	/* restore our old vector */
#else /* USE_NEW_INT */
      SETIVEC(0x10, BIOSSEG, INT_OFF(0x10));	/* restore our old vector */
#endif /* USE_NEW_INT */
      config.vga = 0;
    } else {
      unsigned char *ssp;
      unsigned long sp;

      if (!config.mapped_bios) {
	error("CAN'T DO VIDEO INIT, BIOS NOT MAPPED!\n");
	return 1;
      }
      if (set_ioperm(0x3b0, 0x3db - 0x3b0, 1))
	warn("couldn't get range!\n");
      config.vga = 1;
      set_vc_screen_page(READ_BYTE(BIOS_CURRENT_SCREEN_PAGE));
      warn("WARNING: jumping to 0[c/e]000:0003\n");

      ssp = (unsigned char *) (REG(ss) << 4);
      sp = (unsigned long) LWORD(esp);
      pushw(ssp, sp, LWORD(cs));
      pushw(ssp, sp, LWORD(eip));
      precard_eip = LWORD(eip);
      precard_cs = LWORD(cs);
      LWORD(esp) -= 4;
      LWORD(cs) = config.vbios_seg;
      LWORD(eip) = 3;
      show_regs(__FILE__, __LINE__);
      card_init = 1;
    }

  case DOS_HELPER_SHOW_BANNER:		/* show banner */
    p_dos_str("\n\nLinux DOS emulator " VERSTR " $Date: " VERDATE " $\n");
    p_dos_str("Last configured at %s on %s\n", CONFIG_TIME, CONFIG_HOST);
    p_dos_str("This is work in progress.\n");
    p_dos_str("Please test against a recent version before reporting bugs and problems.\n");
    /* p_dos_str("Formerly maintained by Robert Sanders, gt8134b@prism.gatech.edu\n\n"); */
    p_dos_str("Bugs, Patches & New Code to linux-msdos@vger.rutgers.edu\n\n");
    if (config.dpmi)
      p_dos_str("DPMI-Server Version 0.9 installed\n\n");
    break;

   case DOS_HELPER_INSERT_INTO_KEYBUFFER:
      k_printf("KBD: WARNING: outdated keyboard helper fn 6 was called!\n");
      break;

   case DOS_HELPER_GET_BIOS_KEY:                /* INT 09 "get bios key" helper */
      _AX=get_bios_key();
      k_printf("HELPER: get_bios_key() returned %04x\n",_AX);
      break;
     
  case DOS_HELPER_VIDEO_INIT:
    v_printf("Starting Video initialization\n");
    /* DANG_BEGIN_REMARK
     * Some video BIOSes need access to the PIT timer 2, and some
     * (e.g. Matrox) directly read the timer output on port 0x61.
     * If we don't allow video port access, this will be totally
     * emulated; else, we give temporary access to the needed ports
     * (timer at 0x42, timer config at 0x43 and timer out/speaker at 0x61),
     * provided they were not previously enabled by SPKR_NATIVE - AV
     * DANG_END_REMARK
     */
    if (config.allowvideoportaccess) {
      if (config.speaker != SPKR_NATIVE) {
	v_printf("Giving temporary access to PIT#2\n");
	set_ioperm(0x42, 2, 1);		/* port 0x43 too! */
	set_ioperm(0x61, 1, 1);
      }
      in_video = 1;
    }
    /* DANG_BEGIN_REMARK
     * Many video BIOSes use hi interrupt vector locations as
     * scratchpad area - this is because they come before DOS and feel
     * safe to do it. But we are initializing vectors before video, so
     * this only causes trouble. I assume no video BIOS will ever:
     * - change vectors < 0xe0 (0:380-0:3ff area)
     * - change anything in the vector area _after_ installation - AV
     * DANG_END_REMARK
     */
    v_printf("Save hi vector area\n");
    MEMCPY_2UNIX(save_hi_ints,0x380,128);
    break;

  case DOS_HELPER_VIDEO_INIT_DONE:
    v_printf("Finished with Video initialization\n");
    if (config.allowvideoportaccess) {
      if (config.speaker != SPKR_NATIVE) {
        v_printf("Removing temporary access to PIT#2\n");
        set_ioperm(0x42, 2, 0);
        set_ioperm(0x61, 1, 0);
      }
      in_video = 0;
    }
    v_printf("Restore hi vector area\n");
    MEMCPY_2DOS(0x380,save_hi_ints,128);
    config.emuretrace <<= 1;
    emu_video_retrace_on();
    break;

  case DOS_HELPER_GET_DEBUG_STRING:
    /* TRB - handle dynamic debug flags in dos_helper() */
    LWORD(eax) = GetDebugFlagsHelper((char *) (((_regs.es & 0xffff) << 4) +
					       (_regs.edi & 0xffff)), 1);
    g_printf("DBG: Get flags\n");
    break;

  case DOS_HELPER_SET_DEBUG_STRING:
    g_printf("DBG: Set flags\n");
    LWORD(eax) = SetDebugFlagsHelper((char *) (((_regs.es & 0xffff) << 4) +
					       (_regs.edi & 0xffff)));
    g_printf("DBG: Flags set\n");
    break;

  case DOS_HELPER_SET_HOGTHRESHOLD:
    g_printf("IDLE: Setting hogthreshold value to %u\n", LWORD(ebx));
    config.hogthreshold = LWORD(ebx);
    break;

  case DOS_HELPER_MFS_HELPER:
    mfs_inte6();
    return 1;

  case DOS_HELPER_DOSC:
    if (HI(ax) == 0xdc) {
      /* install check and notify */
      if (!dosc_interface()) return 0;
      running_DosC = LWORD(ebx);
      return 1;
    }
    if (running_DosC) {
      return dosc_interface();
    }
    return 0;

  case DOS_HELPER_EMS_HELPER:
    ems_helper();
    return 1;

  case DOS_HELPER_EMS_BIOS:
  {
    unsigned char *ssp;
    unsigned long sp;

    ssp = (unsigned char *) (REG(ss) << 4);
    sp = (unsigned long) LWORD(esp);

    LWORD(eax) = popw(ssp, sp);
    LWORD(esp) += 2;
    E_printf("EMS: in 0xe6,0x22 handler! ax=0x%04x, bx=0x%04x, dx=0x%04x, "
	     "cx=0x%04x\n", LWORD(eax), LWORD(ebx), LWORD(edx), LWORD(ecx));
    if (config.ems_size)
      ems_fn(&REGS);
    else{
      error("EMS: not running ems_fn!\n");
      return 0;
    }
    break;
  }

  case DOS_HELPER_GARROT_HELPER:             /* Mouse garrot helper */
    if (!LWORD(ebx))   /* Wait sub-function requested */
      usleep(INT28_IDLE_USECS);
    else {             /* Get Hogthreshold value sub-function*/
      LWORD(ebx)= config.hogthreshold;
      LWORD(eax)= config.hogthreshold;
    }
    break;

  case DOS_HELPER_SERIAL_HELPER:   /* Serial helper */
    serial_helper();
    break;

  case DOS_HELPER_BOOTDISK:	/* set/reset use bootdisk flag */
    use_bootdisk = LO(bx) ? 1 : 0;
    break;

  case DOS_HELPER_MOUSE_HELPER:	/* set mouse vector */
    mouse_helper();
    break;

  case DOS_HELPER_CDROM_HELPER:{
      E_printf("CDROM: in 0x40 handler! ax=0x%04x, bx=0x%04x, dx=0x%04x, "
	       "cx=0x%04x\n", LWORD(eax), LWORD(ebx), LWORD(edx), LWORD(ecx));
      cdrom_helper();
      break;
    }

  case DOS_HELPER_ASPI_HELPER: {
#ifdef ASPI_SUPPORT
      extern void aspi_helper(int);
      A_printf("ASPI: in 0x41 handler! ax=0x%04x, bx=0x%04x, dx=0x%04x, "
           "cx=0x%04x\n", LWORD(eax), LWORD(ebx), LWORD(edx), LWORD(ecx));
      aspi_helper(HI(ax));
#else
      LWORD(eax) = 0;
#endif
      break;
  }

  case DOS_HELPER_RUN_UNIX:
    g_printf("Running Unix Command\n");
    run_unix_command(SEG_ADR((char *), es, dx));
    break;   

  case DOS_HELPER_GET_USER_COMMAND:
    /* Get DOS command from UNIX in es:dx (a null terminated buffer) */
    g_printf("Locating DOS Command\n");
    LWORD(eax) = misc_e6_commandline(SEG_ADR((char *), es, dx));
    break;   

  case DOS_HELPER_GET_UNIX_ENV:
    /* Interrogate the UNIX environment in es:dx (a null terminated buffer) */
    g_printf("Interrogating UNIX Environment\n");
    LWORD(eax) = misc_e6_envvar(SEG_ADR((char *), es, dx));
    break;   

  case DOS_HELPER_0x53:
    {
        extern int run_system_command(char *);
        LWORD(eax) = run_system_command(SEG_ADR((char *), es, dx));
	break;
    }

  case DOS_HELPER_GET_CPU_SPEED:
    {
	if (config.rdtsc)
		REG(eax) = (LLF_US << 16) / config.cpu_spd;
	else	REG(eax) = 0;
	break;
    }

  case DOS_HELPER_GET_TERM_TYPE:
    {
	int i;

	/* NOTE: we assume terminal/video init has completed before coming here */
	if (config.X) i = 2;			/* X keyboard */
	else if (config.console_keyb) i = 0;	/* raw keyboard */
	else i = 1;				/* Slang keyboard */

	if (config.console_video) i |= 0x10;
	if (config.graphics)      i |= 0x20;
	if (config.dualmon)       i |= 0x40;
	LWORD(eax) = i;
	break;
    }

#ifdef IPX
  case DOS_HELPER_IPX_CALL:
    if (config.ipxsup) {
      /* TRB handle IPX far calls in dos_helper() */
      IPXFarCallHandler();
    }
    break;
  case DOS_HELPER_IPX_ENDCALL:
    if (config.ipxsup) {
      /* Allow notification of ESR etc... ends */
      IPXEndCall();
    }
    break;
#endif
    case DOS_HELPER_GETCWD:
        LWORD(eax) = (short)((int)getcwd(SEG_ADR((char *), es, dx), (size_t)LWORD(ecx)));
        break;
  case DOS_HELPER_CHDIR:
        LWORD(eax) = chdir(SEG_ADR((char *), es, dx));
        break;
#ifdef X86_EMULATOR
  case DOS_HELPER_CPUEMUON:
#ifdef DONT_DEBUG_BOOT
	memcpy(&d,&d_save,sizeof(struct debug_flags));
#endif
	/* we could also enter from inside dpmi, provided we already
	 * mirrored the LDT into the emu's own one */
  	if ((config.cpuemu==1) && !in_dpmi) enter_cpu_emu();
        break;
  case DOS_HELPER_CPUEMUOFF:
  	if ((config.cpuemu>1) && !in_dpmi) leave_cpu_emu();
        break;
#endif
    case DOS_HELPER_XCONFIG:
#if X_GRAPHICS
	if (config.X) {
		LWORD(eax) = X_change_config((unsigned) LWORD(edx), SEG_ADR((void *), es, bx));
	} else 
#endif /* X_GRAPHICS */
	{
		_AX = -1;
	}
        break;
  case DOS_HELPER_MBR:
    if (LWORD(eax) == 0xfffe) {
      process_master_boot_record();
      break;
    }
    LWORD(eax) = 0xffff;
    /* ... and fall through */
  case DOS_HELPER_EXIT:
    if (LWORD(eax) == DOS_HELPER_REALLY_EXIT) {
      /* terminate code is in bx */
      dbug_printf("DOS termination requested\n");
      p_dos_str("\n\rLeaving DOS...\n\r");
      leavedos(LO(bx));
    }
    break;

  /* here we try to hook a possible plugin */
  #include "plugin_inte6.h"

  default:
    error("bad dos helper function: AX=0x%04x\n", LWORD(eax));
    return 0;
  }

  return 1;
}

#ifndef USE_NEW_INT
static void int08(u_char i)
{
  real_run_int(0x1c);
  /* REG(eflags) |= VIF; */
  WRITE_FLAGSE(READ_FLAGSE() | VIF);
  return;
}
#endif /* not USE_NEW_INT */

static void int15(u_char i)
{
  int num;

  if (HI(ax) != 0x4f)
    NOCARRY;
  /* REG(eflags) |= VIF;
  WRITE_FLAGSE(READ_FLAGSE() | VIF);
  */

  switch (HI(ax)) {
  case 0x10:			/* TopView/DESQview */
    switch (LO(ax))
    {
    case 0x00:			/* giveup timeslice */
      if (config.hogthreshold)
        usleep(INT15_IDLE_USECS);
      return;
    }
    CARRY;
    break;
  case 0x41:			/* wait on external event */
    break;
  case 0x4f:			/* Keyboard intercept */
    HI(ax) = 0x86;
    /*k_printf("INT15 0x4f CARRY=%x AX=%x\n", (LWORD(eflags) & CF),LWORD(eax));*/
    k_printf("INT15 0x4f CARRY=%x AX=%x\n", (READ_FLAGS() & CF),LWORD(eax));
    CARRY;
/*
    if (LO(ax) & 0x80 )
      if (1 || !(LO(ax)&0x80) ){
	fprintf(stderr, "Carrying it out\n");
        CARRY;
      }
      else
	NOCARRY;
*/
    break;
  case 0x80:		/* default BIOS hook: device open */
  case 0x81:		/* default BIOS hook: device close */
  case 0x82:		/* default BIOS hook: program termination */
    HI(ax) = 0;
  case 0x83:
    h_printf("int 15h event wait:\n");
    show_regs(__FILE__, __LINE__);
    CARRY;
    return;			/* no event wait */
  case 0x84:
#ifdef USE_MRP_JOYSTICK
    mrp_read_joystick ();
    h_printf ("int 15h joystick int: %d %d\n", (int)(LWORD(eax)), (int)(LWORD(ebx)));
    return;
#else
    CARRY;
    return;			/* no joystick */
#endif
  case 0x85:
    num = LWORD(eax) & 0xFF;	/* default bios handler for sysreq key */
    if (num == 0 || num == 1) {
      LWORD(eax) &= 0x00FF;
      return;
    }
    LWORD(eax) &= 0xFF00;
    LWORD(eax) |= 1;
    CARRY;
    return;
  case 0x86:
    /* wait...cx:dx=time in usecs */
    g_printf("doing int15 wait...ah=0x86\n");
    show_regs(__FILE__, __LINE__);
    kill_time((long)((LWORD(ecx) << 16) | LWORD(edx)));
    NOCARRY;
    return;

  case 0x87:
    if (config.xms_size)
      xms_int15();
    else {
      LWORD(eax) &= 0xFF;
      LWORD(eax) |= 0x0300;	/* say A20 gate failed - a lie but enough */
      CARRY;
    }
    return;

  case 0x88:
    if (config.xms_size) {
      xms_int15();
    }
    else {
      LWORD(eax) &= ~0xffff;	/* no extended ram if it's not XMS */
      NOCARRY;
    }
    return;

  case 0x89:			/* enter protected mode : kind of tricky! */
    LWORD(eax) |= 0xFF00;		/* failed */
    CARRY;
    return;
  case 0x90:			/* no device post/wait stuff */
    CARRY;
    return;
  case 0x91:
    CARRY;
    return;
  case 0xbf:			/* DOS/16M,DOS/4GW */
    switch (REG(eax) &= 0x00FF)
      {
        case 0: case 1: case 2:		/* installation check */
        default:
          REG(edx) = 0;
          CARRY;
          return;
      }
    return;
  case 0xc0:
    LWORD(es) = ROM_CONFIG_SEG;
    LWORD(ebx) = ROM_CONFIG_OFF;
    HI(ax) = 0;
    return;
  case 0xc1:
    CARRY;
    return;			/* no ebios area */
  case 0xc2:
    m_printf("PS2MOUSE: Call ax=0x%04x\n", LWORD(eax));
    if (!mice->intdrv)
      if (mice->type != MOUSE_PS2) {
	REG(eax) = 0x0500;        /* No ps2 mouse device handler */
	CARRY;
	return;
      }
                
    switch (REG(eax) &= 0x00FF)
      {
      case 0x0000:                    
	mouse.ps2.state = HI(bx);
	if (mouse.ps2.state == 0)
	  mice->intdrv = FALSE;
	else
	  mice->intdrv = TRUE;
 	HI(ax) = 0;
	NOCARRY;		
 	break;
      case 0x0001:
	HI(ax) = 0;
	LWORD(ebx) = 0xAAAA;    /* we have a ps2 mouse */
	NOCARRY;
	break;
      case 0x0003:
	if (HI(bx) != 0) {
	  CARRY;
	  HI(ax) = 1;
	} else {
	  NOCARRY;
	  HI(ax) = 0;
	}
	break;
      case 0x0004:
	HI(bx) = 0xAA;
	HI(ax) = 0;
	NOCARRY;
	break;
      case 0x0005:			/* Initialize ps2 mouse */
	HI(ax) = 0;
	mouse.ps2.pkg = HI(bx);
	NOCARRY;
	break;
      case 0x0006:
	switch (HI(bx)) {
	case 0x00:
	  LO(bx)  = (mouse.rbutton ? 1 : 0);
	  LO(bx) |= (mouse.lbutton ? 4 : 0);
	  LO(bx) |= 0; 	/* scaling 1:1 */
	  LO(bx) |= 0x20;	/* device enabled */
	  LO(bx) |= 0;	/* stream mode */
	  LO(cx) = 0;		/* resolution, one count */
	  LO(dx) = 0;		/* sample rate */
	  HI(ax) = 0;
	  NOCARRY;
	  break;
	case 0x01:
	  HI(ax) = 0;
	  NOCARRY;
	  break;
	case 0x02:
	  HI(ax) = 1;
	  CARRY;
	  break;
	}
	break;
#if 0
      case 0x0007:
	pushw(ssp, sp, 0x000B);
	pushw(ssp, sp, 0x0001);
	pushw(ssp, sp, 0x0001);
	pushw(ssp, sp, 0x0000);
	REG(cs) = REG(es);
	REG(eip) = REG(ebx);
	HI(ax) = 0;
	NOCARRY;
	break;
#endif
     default:
	HI(ax) = 1;
	g_printf("PS2MOUSE: Unknown call ax=0x%04x\n", LWORD(eax));
	CARRY;
      }
    return;
  case 0xc3:
    /* no watchdog */
    CARRY;
    return;
  case 0xc4:
    /* no post */
    CARRY;
    return;
  case 0xc9:
    if (LO(ax) == 0x10) {
	HI(ax) = 0;
	HI(cx) = vm86s.cpu_type;
	LO(cx) = 0x20;
	return;
    }
  /* else fall through */
  case 0x24:		/* PS/2 A20 gate support */
  case 0xd8:		/* EISA - should be set in config? */
  case 0xda:
  case 0xdb:
	HI(ax) = 0x86;
	break;

  case 0xe8:
    if (LO(ax) == 1) {
	Bit32u mem = ((config.xms_size > config.ems_size) ?
			 config.xms_size : config.ems_size);
	LWORD(eax) = mem;
	LWORD(ebx) = mem >>16;
	NOCARRY;
	return;
    }
    /* Fall through !! */

  default:
    g_printf("int 15h error: ax=0x%04x\n", LWORD(eax));
    CARRY;
    return;
  }
}

void set_ticks(unsigned long new)
{
  volatile unsigned long *ticks = BIOS_TICK_ADDR;
  volatile unsigned char *overflow = TICK_OVERFLOW_ADDR;

  ignore_segv++;
  *ticks = new;
  /* A timer read should reset the overflow flag */
  *overflow = 0;
  ignore_segv--;
  h_printf("TICKS: update ticks to %ld\n", new);
}

static void int1a(u_char i)
{
  time_t time_val;
  struct timeval;
  struct timezone;
  struct tm *tm;

  switch (HI(ax)) {

/*
--------B-1A00-------------------------------
INT 1A - TIME - GET SYSTEM TIME
	AH = 00h
Return: CX:DX = number of clock ticks since midnight
	AL = midnight flag, nonzero if midnight passed since time last read
Notes:	there are approximately 18.2 clock ticks per second, 1800B0h per 24 hrs
	  (except on Tandy 2000, where the clock runs at 20 ticks per second)
	IBM and many clone BIOSes set the flag for AL rather than incrementing
	  it, leading to loss of a day if two consecutive midnights pass
	  without a request for the time (e.g. if the system is on but idle)
->	since the midnight flag is cleared, if an application calls this
->	  function after midnight before DOS does, DOS will not receive the
->	  midnight flag and will fail to advance the date
*/
  case 0:			/* read time counter */

#define BIOSTIMER_ONLY_VIEW
#ifdef BIOSTIMER_ONLY_VIEW

    /* We rely on the INT8 routine doing the right thing,
     * DOS apps too rely on the relationship between INT1A and 0x46c timer.
     * We already do all appropriate things to trigger the simulated INT8
     * correctly (well, sometimes faking it), so the 0x46c timer incremented
     * by the (realmode) INT8 handler should be always in sync.
     * Therefore, we keep INT1A,AH0 simple instead of trying to be too clever;-)
     */
    {
      static int first = 1;
      if (first) {
        /* take over the correct value _once_ only */
        *((unsigned long *)(BIOS_TICK_ADDR)) =
               (unsigned long)(pic_sys_time >> 16)
             + (sys_base_ticks + usr_delta_ticks);
        first = 0;
      }
    }
    last_ticks = *((unsigned long *)(BIOS_TICK_ADDR));
    LO(ax) = *((u_char *)(TICK_OVERFLOW_ADDR));
    LWORD(ecx) = (last_ticks >> 16) & 0xffff;
    LWORD(edx) = last_ticks & 0xffff;
    *((u_char *)(TICK_OVERFLOW_ADDR)) = 0; /* clear the midnight flag */

#else /* not BIOSTIMER_ONLY_VIEW */

    /* pic_sys_time is a zero-based tick(1.19MHz) counter. As such, if we
     * shift it right by 16 we get the number of PIT0 overflows, that is,
     * the number of 18.2ms timer ticks elapsed since starting dosemu. This
     * is independent of any int8 speedup a program can set, since the PIT0
     * counting frequency is fixed. The count overflows after 7 1/2 years.
     * usr_delta_ticks is 0 as long as nobody sets a new time (B-1A01)
     */
#ifdef DEBUG_INT1A
    g_printf("TIMER: sys_base_ticks=%ld usr_delta_ticks=%ld pic_sys_time=%#Lx\n",
    	sys_base_ticks, usr_delta_ticks, pic_sys_time);
#endif
    last_ticks = (unsigned long)(pic_sys_time >> 16);
    last_ticks += (sys_base_ticks + usr_delta_ticks);

    /* has the midnight passed? */
    if (last_ticks > TICKS_IN_A_DAY) {
      *((u_char *)(TICK_OVERFLOW_ADDR)) |= 0x1;
      last_ticks -= TICKS_IN_A_DAY;
      /* since pic_sys_time continues to increase, avoid further midnight
       * overflows */
      sys_base_ticks -= TICKS_IN_A_DAY;
    }
    LO(ax) = *((u_char *)(TICK_OVERFLOW_ADDR));
    LWORD(ecx) = (last_ticks >> 16) & 0xffff;
    LWORD(edx) = last_ticks & 0xffff;

#ifdef DEBUG_INT1A
    if (d.general) {
      long k = last_ticks/18.2065;	/* sorry */
      time(&time_val);
      g_printf("INT1A: read timer = %ld (%ld:%ld:%ld) %s\n", last_ticks,
      	k/3600, (k%3600)/60, (k%60), ctime(&time_val));
    }
#else
    g_printf("INT1A: read timer=%ld, midnight=%d\n", last_ticks, LO(ax));
#endif

    set_ticks(last_ticks);	/* set_ticks is in rtc.c */
#endif /* not BIOSTIMER_ONLY_VIEW */
    break;

/*
--------B-1A01-------------------------------
INT 1A - TIME - SET SYSTEM TIME
	AH = 01h
	CX:DX = number of clock ticks since midnight
Return: nothing
Notes:	there are approximately 18.2 clock ticks per second, 1800B0h per 24 hrs
	  (except on Tandy 2000, where the clock runs at 20 ticks per second)
	this call resets the midnight-passed flag
SeeAlso: AH=00h,AH=03h,INT 21/AH=2Dh
*/
  case 1:			/* write time counter */
    {
      /* get current system time and check it (previous usr_delta could
       * be != 0) */
      long t;
      do {
        t = (pic_sys_time >> 16) + sys_base_ticks;
        if (t < 0) sys_base_ticks += TICKS_IN_A_DAY;
      } while (t < 0);

      /* get user-requested time */
      last_ticks = (LWORD(ecx) << 16) | (LWORD(edx) & 0xffff);

      usr_delta_ticks = last_ticks - t;
      *(u_char *) (TICK_OVERFLOW_ADDR) = 0;
#ifdef DEBUG_INT1A
      g_printf("TIMER: sys_base_ticks=%ld usr_delta_ticks=%ld pic_sys_time=%#Lx\n",
    	sys_base_ticks, usr_delta_ticks, pic_sys_time);
#endif
      set_ticks(last_ticks);
      g_printf("INT1A: set timer to %ld\n", last_ticks);
      break;
    }
/*
--------B-1A02-------------------------------
INT 1A - TIME - GET REAL-TIME CLOCK TIME (AT,XT286,PS)
	AH = 02h
Return: CF clear if successful
	    CH = hour (BCD)
	    CL = minutes (BCD)
	    DH = seconds (BCD)
	    DL = daylight savings flag (00h standard time, 01h daylight time)
	CF set on error (i.e. clock not running or in middle of update)
Note:	this function is also supported by the Sperry PC, which predates the
	  IBM AT; the data is returned in binary rather than BCD on the Sperry,
	  and DL is always 00h
SeeAlso: AH=00h,AH=03h,AH=04h,INT 21/AH=2Ch
*/
  case 2:			/* get time */
    LOCK_CMOS;
    HI(cx) = BCD(GET_CMOS(CMOS_HOUR));
    LO(cx) = BCD(GET_CMOS(CMOS_MIN));
    HI(dx) = BCD(GET_CMOS(CMOS_SEC));
    UNLOCK_CMOS;
    g_printf("INT1A: RTC time %02x:%02x:%02x\n",HI(cx),LO(cx),HI(dx));
    NOCARRY;
    break;

/*
--------B-1A04-------------------------------
INT 1A - TIME - GET REAL-TIME CLOCK DATE (AT,XT286,PS)
	AH = 04h
Return: CF clear if successful
	    CH = century (BCD)
	    CL = year (BCD)
	    DH = month (BCD)
	    DL = day (BCD)
	CF set on error
SeeAlso: AH=02h,AH=04h"Sperry",AH=05h,INT 21/AH=2Ah,INT 4B/AH=02h"TI"
*/
  case 4:			/* get date */
    time(&time_val);
    tm = localtime((time_t *) &time_val);
    tm->tm_year += 1900;
    tm->tm_mon++;
    LWORD(ecx) = tm->tm_year % 10;
    tm->tm_year /= 10;
    LWORD(ecx) |= (tm->tm_year % 10) << 4;
    tm->tm_year /= 10;
    LWORD(ecx) |= (tm->tm_year % 10) << 8;
    tm->tm_year /= 10;
    LWORD(ecx) |= (tm->tm_year) << 12;
    LO(dx) = tm->tm_mday % 10;
    tm->tm_mday /= 10;
    LO(dx) |= tm->tm_mday << 4;
    HI(dx) = tm->tm_mon % 10;
    tm->tm_mon /= 10;
    HI(dx) |= tm->tm_mon << 4;
    /* REG(eflags) &= ~CF; */
    g_printf("INT1A: RTC date %04x%02x%02x (DOS format)\n", _CX, _DH, _DL);
    NOCARRY;
    break;

/*
--------B-1A03-------------------------------
INT 1A - TIME - SET REAL-TIME CLOCK TIME (AT,XT286,PS)
	AH = 03h
	CH = hour (BCD)
	CL = minutes (BCD)
	DH = seconds (BCD)
	DL = daylight savings flag (00h standard time, 01h daylight time)
Return: nothing
Note:	this function is also supported by the Sperry PC, which predates the
	  IBM AT; the data is specified in binary rather than BCD on the
	  Sperry, and the value of DL is ignored
--------B-1A05-------------------------------
INT 1A - TIME - SET REAL-TIME CLOCK DATE (AT,XT286,PS)
	AH = 05h
	CH = century (BCD)
	CL = year (BCD)
	DH = month (BCD)
	DL = day (BCD)
Return: nothing
*/
  case 3:			/* set time */
  case 5:			/* set date */
    g_printf("INT1A: RTC: can't set time/date\n");
    break;

  /* Notes: the alarm occurs every 24 hours until turned off, invoking INT 4A
  	each time the BIOS does not check for invalid values for the time, so
  	the CMOS clock chip's "don't care" setting (any values between C0h
  	and FFh) may be used for any or all three parts.  For example, to
  	create an alarm once a minute, every minute, call with CH=FFh, CL=FFh,
  	and DH=00h. (R.Brown)
   */
  case 6:			/* set alarm */
    {
      int stb;
      unsigned char h,m,s;

      LOCK_CMOS;
      stb=GET_CMOS(CMOS_STATUSB);
      if (stb&0x20) {
        CARRY;
      } else {
	/* bit 2 of register 0xb set=binary mode, clear=BCD mode */
	if (stb & 4) {
          SET_CMOS(CMOS_HOURALRM, (h=_CH));
          SET_CMOS(CMOS_MINALRM, (m=_CL));
          SET_CMOS(CMOS_SECALRM, (s=_DH));
	}
	else {
          SET_CMOS(CMOS_HOURALRM, (h=BIN(_CH)));
          SET_CMOS(CMOS_MINALRM, (m=BIN(_CL)));
          SET_CMOS(CMOS_SECALRM, (s=BIN(_DH)));
	}
        r_printf("RTC: set alarm to %02d:%02d:%02d\n",h,m,s);  /* BIN! */
        /* This has been VERIFIED on an AMI BIOS -- AV */
        SET_CMOS(CMOS_STATUSB, stb|0x20);
        clear_bit (PIC_IRQ8, &pic1_imr);
        NOCARRY;
      }
      UNLOCK_CMOS;
      break;
    }

  case 7:			/* clear alarm but NOT PIC mask */
    /* This has been VERIFIED on an AMI BIOS -- AV */
    LOCK_CMOS;
    SET_CMOS(CMOS_STATUSB, GET_CMOS(CMOS_STATUSB)&~0x20);
    UNLOCK_CMOS;
#ifdef NEW_PIC
    pic_untrigger(PIC_IRQ8);
#endif
    break;
 
  case 0xb1:			/* Intel PCI BIOS v 2.0c */
      pci_bios();
    break;

  default:
    g_printf("WARNING: unsupported INT0x1a call 0x%02x\n", HI(ax));
    CARRY;
    return;
  }
}

/* ========================================================================= */
/*
 * DANG_BEGIN_FUNCTION ms_dos
 *
 * int0x21 call
 *
 * we trap this for two functions: simulating the EMMXXXX0 device and
 * fudging the CONFIG.XXX and AUTOEXEC.XXX bootup files.
 *
 * note that the emulation herein may cause problems with programs
 * that like to take control of certain int 21h functions, or that
 * change functions that the true int 21h functions use.  An example
 * of the latter is ANSI.SYS, which changes int 10h, and int 21h
 * uses int 10h.  for the moment, ANSI.SYS won't work anyway, so it's
 * no problem.
 *
 * DANG_END_FUNCTION
 */
/* XXX - MAJOR HACK!!! this is bad bad wrong.  But it'll probably work
 * unless someone puts "files=200" in his/her config.sys
 */
#define EMM_FILE_HANDLE 200

/* uppercase and truncate to 3 letters the replacement extension */
/* pay attention to writable strings! */
#define ext_fix(s) { char *r=(s); \
		     while (*r) { *r=toupper(*r); r++; } \
		     if ((r - s) > 3) s[3]=0; }

static int ms_dos(int nr)
{
  switch (nr) {
  case 0x3d:       /* DOS handle open */
  {
    char *ptr = SEG_ADR((char *), ds, dx);

    /* ignore explicitly selected drive by incrementing ptr by 1 */
    if (config.emubat && !strncasecmp(ptr + 1, ":\\AUTOEXEC.BAT", 14)) {
      ext_fix(config.emubat);
      sprintf(ptr + 1, ":\\AUTOEXEC.%-3s", config.emubat);
      d_printf("DISK: Substituted %s for AUTOEXEC.BAT\n", ptr + 1);
    } else if (config.emusys && !strncmp(ptr, "\\CONFIG.SYS", 11)) {
      ext_fix(config.emusys);
      sprintf(ptr, "\\CONFIG.%-3s", config.emusys);
      d_printf("DISK: Substituted %s for CONFIG.SYS\n", ptr);
 } else if (config.emuini && !strncmp(ptr+2, "\\WINDOWS\\SYSTEM.INI", 19)) {
 ext_fix(config.emuini);
      sprintf(ptr+2, "\\WINDOWS\\SYSTEM.%s", config.emuini);
      d_printf("DISK: Substituted %s for system.ini\n", ptr);
    }
#ifdef INTERNAL_EMS
    else if (config.ems_size && !strncmp(ptr, "EMMXXXX0", 8)) {
      E_printf("EMS: opened EMM file!\n");
      LWORD(eax) = EMM_FILE_HANDLE;
      NOCARRY;
      show_regs(__FILE__, __LINE__);
      return 1;
    }
#endif

  return 0;
  }

#ifdef INTERNAL_EMS
  case 0x3e:       /* DOS handle close */
    if ((LWORD(ebx) != EMM_FILE_HANDLE) || !config.ems_size)
      return 0;
    else {
      E_printf("EMS: closed EMM file!\n");
      NOCARRY;
      show_regs(__FILE__, __LINE__);
      return 1;
    }

  case 0x44:      /* DOS ioctl */
    if ((LWORD(ebx) != EMM_FILE_HANDLE) || !config.ems_size)
      return 0;

    switch (LO(ax)) {
    case 0:     /* get device info */
      E_printf("EMS: dos_ioctl getdevinfo emm.\n");
      LWORD(edx) = 0x80;
      NOCARRY;
      show_regs(__FILE__, __LINE__);
      return 1;
      break;
    case 7:     /* check output status */
      E_printf("EMS: dos_ioctl chkoutsts emm.\n");
      LO(ax) = 0xff;
      NOCARRY;
      show_regs(__FILE__, __LINE__);
      return 1;
      break;
    }
  error("dos_ioctl shouldn't get here. XXX\n");
  return 0;
#endif

  case 0x2C:                    /* get time & date */
    if (config.hogthreshold)    /* allow linux to idle */
      usleep(INT2F_IDLE_USECS);
    return 0;

  default:
#ifndef USE_NEW_INT
    if (!in_dpmi)
      g_printf("INT21 (0x%02x):  we shouldn't be here! ax=0x%04x, bx=0x%04x\n",
	     nr, LWORD(eax), LWORD(ebx));
#endif
    return 0;
  }
  return 1;
}

/* ========================================================================= */

void real_run_int(int i)
{
  unsigned char *ssp;
  unsigned long sp;

#ifndef USE_NEW_INT
  /* ssp = (unsigned char *)(REG(ss)<<4); */
  ssp = (unsigned char *)(READ_SEG_REG(ss)<<4);
  sp = (unsigned long) LWORD(esp);

  pushw(ssp, sp, vflags);
  /* pushw(ssp, sp, LWORD(cs)); */
  pushw(ssp, sp, READ_SEG_REG(cs));
  pushw(ssp, sp, LWORD(eip));
  LWORD(esp) -= 6;
  /* LWORD(cs) = ((us *) 0)[(i << 1) + 1]; */
  WRITE_SEG_REG(cs, ((us *) 0)[(i << 1) + 1]);
  LWORD(eip) = ((us *) 0)[i << 1];

#else /* USE_NEW_INT */
  ssp = (unsigned char *)(_SS<<4);
  sp = (unsigned long) _SP;

  pushw(ssp, sp, read_FLAGS());
  pushw(ssp, sp, _CS);
  pushw(ssp, sp, _IP);
  _SP -= 6;
  _CS = ISEG(i);
  _IP = IOFF(i);
#endif /* USE_NEW_INT */

  /* clear TF (trap flag, singlestep), VIF/IF (interrupt flag), and
   * NT (nested task) bits of EFLAGS
   * NOTE: IF-flag only, because we are not sure that we will test it in
   *       some of our own software (...we all are human beings)
   *       For vm86() 'VIF' is the candidate to reset in order to do CLI !
   */
#ifndef USE_NEW_INT
  WRITE_FLAGSE(READ_FLAGSE() & ~(VIF | TF | IF | NT));
#else /* USE_NEW_INT */
  clear_TF();
  clear_NT();
  clear_IF();
#endif /* USE_NEW_INT */
}

#ifdef USE_NEW_INT
/* DANG_BEGIN_FUNCTION run_caller_func(i, from_int)
 *
 * This function runs the specified caller function in response to an
 * int instruction.  Where i is the interrupt function to execute and
 * from_int specifies if we are comming directly from an int
 * instruction.
 *
 * This function runs the instruction with the following model _CS:_IP is the
 * address to start executing at after the caller function terminates, and
 * _EFLAGS are the flags to use after termination.  For the simple case of an
 * int instruction this is easy.  _CS:_IP = retCS:retIP and _FLAGS = retFLAGS
 * as well equally the current values (retIP = curIP +2 technically).
 *
 * However if the function is called (from dos) by simulating an int instruction
 * (something that is common with chained interrupt vectors) 
 * _CS:_IP = BIOS_SEG:HLT_OFF(i) and _FLAGS = curFLAGS 
 * while retCS, retIP, and retFlags are on the stack.  These I pop and place in
 * the appropriate registers.  
 *
 * This functions actions certainly correct for functions executing an int/iret
 * discipline.  And almost certianly correct for functions executing an
 * int/retf#2 discipline (with flag changes), as an iret is always possilbe.
 * However functions like dos int 0x25 and 0x26 executing with a int/retf will
 * not be handled correctlty by this function and if you need to handle them
 * inside dosemu use a halt handler instead.
 *
 * Finally there is a possible trouble spot lurking in this code.  Interrupts
 * are only implicitly disabled when it calls the caller function, so if for
 * some reason the main loop would be entered before the caller function returns
 * wrong code may execute if the retFLAGS have interrupts enabled!  
 *
 * This is only a real handicap for sequences of dosemu code execute for long
 * periods of time as we try to improve timer response and prevent signal queue
 * overflows! -- EB 10 March 1997
 *
 * Grumble do to code that executes before interrupts, and the
 * semantics of default_interupt, I can't implement this function as I
 * would like.  In the tricky case of being called from dos by
 * simulating an int instruction, I must leave retCS, retIP, on the
 * stack.  But I can safely read retFlags so I do.  
 * I pop retCS, and retIP just before returning to dos, as well as
 * dropping the stack slot  that held retFlags.
 *
 * This improves consistency of interrupt handling, but not quite as
 * much as if I could handle it the way I would like.
 * -- EB 30 Nov 1997
 *
 * DANG_END_FUNCTION
 */

static void run_caller_func(int i, Boolean from_int)
{
	void (*caller_function)(int i);
	ifprintf((d.dos > 1), "DO_INT0x%02x: Using caller_function()\n", i);

#if 0	 /* This causes problems with default_interrupt disable this for now
          * --EB 13 March 1997 
	  */

	if (!from_int) {
		_IP = POPW(_SS, _SP);
		_CS = POPW(_SS, _SP);
		set_FLAGS(POPW(_SS, _SP));
	}
#else
	/* Do to a misfeature I must leave retCS & retIP on the stack, but I
	 * can still read retFlags.
	 */
	if (!from_int) {
		unsigned char *ssp = (unsigned char *)(_SS<<4);
		unsigned long sp = (Bit16u)(_SP +4);
		set_FLAGS(popw(ssp, sp));
	}
#endif
	caller_function = interrupt_function[i];
	if (caller_function) {

		caller_function(i);
	}
	if (!from_int) {
		unsigned char *ssp = (unsigned char *)(_SS<<4);
		unsigned long sp = _SP;
		_SP += 6;
		_IP = popw(ssp, sp);
		_CS = popw(ssp, sp);
		/* set_FLAGS(popw(ssp, sp)); */
	}
}
#endif /* USE_NEW_INT */

int can_revector(int i)
{
/* here's sort of a guideline:
 * if we emulate it completely, but there is a good reason to stick
 * something in front of it, and it seems to work, by all means revector it.
 * if we emulate none of it, say yes, as that is a little bit faster.
 * if we emulate it, but things don't seem to work when it's revectored,
 * then don't let it be revectored.
 */

  switch (i) {
  case 0: case 4: case 5: case 7:
#if 1 /* temp fix for bug in kernel vm86plus code
       * (fixing kernel patch is in 2.1.27 and will appear in 2.0.30, as Linus promised)
       * we will remove this, when time goes by --Hans 970225
       */
    if (running_kversion > 2001026 || (running_kversion > 2000029 && running_kversion < 2000000) )
      return NO_REVECT;
#endif
  case 0x21:			/* we want it first...then we'll pass it on */
#ifdef DJGPP_HACK
  case 0x23:			/* TMP FIX for ^C under DPMI */
#endif
  case 0x28:                    /* keyboard idle interrupt */
  case 0x2f:			/* needed for XMS, redirector, and idling */
  case DOS_HELPER_INT:		/* e6 for redirector and helper (was 0xfe) */
  case 0xe7:			/* for mfs FCB helper */
#ifdef USE_INT_QUEUE
  case 0xe8:			/* for int_queue_run return */
#endif
    return REVECT;

  case 0x74:			/* needed for PS/2 Mouse */
    if ((mice->type == MOUSE_PS2) || (mice->intdrv))
      return REVECT;
    else
      return NO_REVECT;

  case 0x33:			/* Mouse. Wrapper for mouse-garrot as well*/
    if (mice->intdrv || config.hogthreshold)
      return REVECT;
    else
      return NO_REVECT;

#if 0		/* no need to specify all */
  case 0x10:			/* BIOS video */
  case 0x13:			/* BIOS disk */
  case 0x15:
  case 0x16:			/* BIOS keyboard */
  case 0x17:			/* BIOS printer */
  case 0x1b:			/* ctrl-break handler */
  case 0x1c:			/* user timer tick */
  case 0x20:			/* exit program */
  case 0x25:			/* absolute disk read, calls int 13h */
  case 0x26:			/* absolute disk write, calls int 13h */
  case 0x27:			/* TSR */
  case 0x2a:
  case 0x60:
  case 0x61:
  case 0x62:
  case 0x67:			/* EMS */
  case 0xfe:			/* Turbo Debugger and others... */
#endif
  default:
    return NO_REVECT;
  }
}

static int can_revector_int21(int i)
{
  switch (i) {
  case 0x2c:          /* get time */
#ifdef INTERNAL_EMS
  case 0x3e:          /* dos handle close */
  case 0x44:          /* dos ioctl */
#endif
    return REVECT;

  case 0x3d:          /* dos handle open */
    if (config.emusys || config.emubat)
      return REVECT;
    else
      return NO_REVECT;

  default:
    return NO_REVECT;      /* don't emulate most int 21h functions */
  }
}

static void int05(u_char i) {
    g_printf("BOUNDS exception\n");
    default_interrupt(i);
    return;
}

#ifndef USE_NEW_INT
void int_a_b_c_d_e_f(u_char i) {
    g_printf("IRQ->interrupt %x\n", i);
    show_regs(__FILE__, __LINE__);
    default_interrupt(i);
    return;
}

/* IRQ1, keyb data ready */
static void int09(u_char i) {
    fprintf(stderr, "IRQ->interrupt %x\n", i);
    real_run_int(0x09);
    return;
}
#endif /* not USE_NEW_INT */

/* CONFIGURATION */
static void int11(u_char i) {
    LWORD(eax) = configuration;
    return;
}

/* MEMORY */
static void int12(u_char i) {
    LWORD(eax) = config.mem_size;
    return;
}

#ifndef USE_NEW_INT
/* KEYBOARD */
static void int16(u_char i) {
    real_run_int(0x16);
    return;
}
#endif /* not USE_NEW_INT */

/* BASIC */
static void int18(u_char i) {
  k_printf("BASIC interrupt being attempted.\n");
}

/* LOAD SYSTEM */
static void int19(u_char i) {
  boot();
}


/*
 * Turn all simulated FAT devices into network drives.
 */
static void redirect_devices()
{
  extern int RedirectDisk(int, char *, int);

  static char s[256] = "\\\\LINUX\\FS", *t = s + 10;
  int i, j;

  for (i = 0; i < MAX_HDISKS; i++) {
    if(hdisktab[i].type == DIR_TYPE && hdisktab[i].fatfs) {
      strncpy(t, hdisktab[i].dev_name, 245);
      s[255] = 0;
      j = RedirectDisk(i + 2, s, hdisktab[i].rdonly);

      ds_printf("INT21: redirecting %c: %s (err = %d)\n", i + 'C', j ? "failed" : "ok", j);
    }
  }
}

/*
 * Activate the redirector just before the first int 21h file open call.
 *
 * To use this feature, set redir_state = 1 and make sure int 21h is
 * revectored.
 */
static int redir_it()
{
  /*
   * Declaring the following struct volatile works around an EGCS bug
   * (at least up to egcs-2.91.66). Otherwise the line below marked
   * with '###' will modify the *old* (saved) struct too.
   * -- sw
   */
  volatile static struct vm86_regs save_regs;
  static unsigned x0, x1, x2, x3, x4;
  unsigned u;

  /*
   * To start up the redirector we need (1) the list of list, (2) the DOS version and
   * (3) the swappable data area. To get these, we reuse the original file open call.
   */
  switch(redir_state) {
    case 1:
      if(HI(ax) == 0x3d) {
        /*
         * FreeDOS will get confused by the following calling sequence (e.i. it
         * is not reentrant 'enough'. So we will abort here - it cannot use a
         * redirector anyway.
         * -- sw
         */
        if (running_DosC) {
          ds_printf("INT21: FreeDOS detected - no check for redirector\n");
          return redir_state = 0;
        }
        save_regs = REGS;
        redir_state = 2;
        LWORD(eip) -= 2;
        LWORD(eax) = 0x5200;		/* ### , see above EGCS comment! */
        default_interrupt(0x21);
        ds_printf("INT21 +1 (%d) at %04x:%04x: AX=%04x, BX=%04x, CX=%04x, DX=%04x, DS=%04x, ES=%04x\n",
          redir_state, LWORD(cs), LWORD(eip), LWORD(eax), LWORD(ebx), LWORD(ecx), LWORD(edx), LWORD(ds), LWORD(es));
        return 1;
      }
      break;

    case 2:
      x0 = LWORD(ebx); x1 = REG(es);
      redir_state = 3;
      LWORD(eip) -= 2;
      LWORD(eax) = 0x3000;
      default_interrupt(0x21);
      ds_printf("INT21 +2 (%d) at %04x:%04x: AX=%04x, BX=%04x, CX=%04x, DX=%04x, DS=%04x, ES=%04x\n",
        redir_state, LWORD(cs), LWORD(eip), LWORD(eax), LWORD(ebx), LWORD(ecx), LWORD(edx), LWORD(ds), LWORD(es));
      return 2;
      break;

    case 3:
      x4 = LWORD(eax);
      redir_state = 4;
      LWORD(eip) -= 2;
      LWORD(eax) = 0x5d06;
      default_interrupt(0x21);
      ds_printf("INT21 +3 (%d) at %04x:%04x: AX=%04x, BX=%04x, CX=%04x, DX=%04x, DS=%04x, ES=%04x\n",
        redir_state, LWORD(cs), LWORD(eip), LWORD(eax), LWORD(ebx), LWORD(ecx), LWORD(edx), LWORD(ds), LWORD(es));
      return 3;
      break;

    case 4:
      x2 = LWORD(esi); x3 = REG(ds);
      redir_state = 0;
      u = x0 + (x1 << 4);
      ds_printf("INT21: lol = 0x%x\n", u);
      ds_printf("INT21: sda = 0x%x\n", x2 + (x3 << 4));
      ds_printf("INT21: ver = 0x%02x\n", x4);

      if(*(unsigned *) (u + 0x16)) {		/* Do we have a CDS entry? */
        /* Init the redirector. */
        LWORD(ecx) = x4;
        LWORD(edx) = x0; REG(es) = x1;
        LWORD(esi) = x2; REG(ds) = x3;
        LWORD(ebx) = 0x500;
        LWORD(eax) = 0x20;
        mfs_inte6();

        redirect_devices();
      }
      else {
        ds_printf("INT21: this DOS has no CDS entry - redirector not used\n");
      }

      REGS = save_regs;
      set_int21_revectored(-1);
      break;
  }

  return 0;
}


/* MS-DOS */
static void int21(u_char i) {
#ifdef X86_EMULATOR
  static char buf[80];
#endif
  ds_printf("INT21 (%d) at %04x:%04x: AX=%04x, BX=%04x, CX=%04x, DX=%04x, DS=%04x, ES=%04x\n",
       redir_state, LWORD(cs), LWORD(eip),
       LWORD(eax), LWORD(ebx), LWORD(ecx), LWORD(edx), LWORD(ds), LWORD(es));

  if(redir_state && redir_it()) return;

#if 1
  if(HI(ax) == 0x3d) {
    char *p = (char *) (((REG(ds)) << 4) + LWORD(edx));
    int i;

    ds_printf("INT21: open file \"");
    for(i = 0; i < 64 && p[i]; i++) ds_printf("%c", p[i]);
    ds_printf("\"\n");
  }
#endif

#ifdef X86_EMULATOR
  if ((HI(ax)==0x40) && LWORD(ecx)) {
	char *dp = (char *)((LWORD(ds)<<4)+LWORD(edx));
	unsigned int nb = LWORD(ecx);
	if (nb>78) nb=78; memcpy(buf,dp,nb); buf[nb]=0;
	ds_printf("WRITE: [%s]\n",buf);
  }
  else if (HI(ax)==9) {
	char *dp = (char *)((LWORD(ds)<<4)+LWORD(edx));
	char *q = buf;
	int nb;
	for (nb=0; (nb<78)&&(*dp!='$'); nb++) *q++ = *dp++;
	buf[nb]=0;
	ds_printf("WRITE: [%s]\n",buf);
  }
  else if ((HI(ax)==6) && (LO(ax)!=0xff)) {
	ds_printf("WRITE: [%c]\n",isprint(LO(ax)? LO(ax):'.'));
  }
#endif
  if (!ms_dos(HI(ax)))
    default_interrupt(i);
}

#ifdef DJGPP_HACK
/* Ctrl-C */
static void int23(u_char i)
{
  /* Had to revector here int0x23 under DPMI to solve the obnoxious
   * case of ^C under djgpp - actually my DOS (IBM 7.0) gets the ^C
   * and shuts down the program without telling it to dosemu :( - AV
   */
  if (in_dpmi)
	NOCARRY;
  else
	real_run_int(0x23);
  return; 
}
#endif

static void mouse_post_boot(void)
{
	extern void bios_f000_int10_old();
	us *ptr;
	/* This is needed here to revectoring the interrupt, after dos
	 * has revectored it. --EB 1 Nov 1997 */

	/* This code is dupped for now in base/mouse/mouse.c - JES 96/10/20 */
	#define Mouse_INT       (0x33 * 16)
	SETIVEC(0x33, Mouse_SEG, Mouse_INT);
#ifndef USE_NEW_INT
      	#define Mouse_INT74     (0x74 * 16)
	SETIVEC(0x74, Mouse_SEG, Mouse_INT74);
#else /* USE_NEW_INT */
#if 0
	SETIVEC(0x74, Mouse_SEG, Mouse_ROUTINE_OFF);
#endif

#endif /* USE_NEW_INT */
      
	/* grab int10 back from video card for mouse */
        ptr = (us*)((BIOSSEG << 4) +
		    ((long)bios_f000_int10_old - (long)bios_f000));
        m_printf("ptr is at %p; ptr[0] = %x, ptr[1] = %x\n",ptr,ptr[0],ptr[1]);
        ptr[0] = IOFF(0x10);
        ptr[1] = ISEG(0x10);
        m_printf("after store, ptr[0] = %x, ptr[1] = %x\n",ptr[0],ptr[1]);
#if defined(USE_NEW_INT) || (INT10_WATCHER_SEG != BIOSSEG)
	 /* Otherwise this isn't safe */
	SETIVEC(0x10, INT10_WATCHER_SEG, INT10_WATCHER_OFF);
#endif
}

static void dos_post_boot(void)
{
    if (!config.keybint && config.console_keyb) {
      /* revector int9, so dos doesn't play with the keybuffer */
      k_printf("revectoring int9 away from dos\n");
      SETIVEC(0x9, BIOSSEG, 16 * 0x8 + 2);  /* point to the IRET before INT9 */
    }
    if (mice->intdrv) mouse_post_boot();

}

/* KEYBOARD BUSY LOOP */
static void int28(u_char i) {
  static int first = 1;
  if (first) {
    first = 0;
    dos_post_boot();
  }

  if (config.hogthreshold) {
    /* the hogthreshold value just got redefined to be the 'garrot' value */
    static int time_count = 0;
    if (++time_count >= config.hogthreshold) {
      usleep(INT28_IDLE_USECS);
      time_count = 0;
    }
  }

  default_interrupt(i);
}

/* FAST CONSOLE OUTPUT */
static void int29(u_char i) {
    /* char in AL */
  char_out(*(char *) &REG(eax), READ_BYTE(BIOS_CURRENT_SCREEN_PAGE));
}

static void int2f(u_char i)
{
#if 1
  ds_printf("INT2F at %04x:%04x: AX=%04x, BX=%04x, CX=%04x, DX=%04x, DS=%04x, ES=%04x\n",
       LWORD(cs), LWORD(eip),
       LWORD(eax), LWORD(ebx), LWORD(ecx), LWORD(edx), LWORD(ds), LWORD(es));
#endif
  switch (LWORD(eax)) {
  case INT2F_IDLE_MAGIC:   /* magic "give up time slice" value */
    if (config.hogthreshold)
      usleep(INT2F_IDLE_USECS);
    LWORD(eax) = 0;
    return;

#ifdef IPX
  case INT2F_DETECT_IPX:  /* TRB - detect IPX in int2f() */
    if (config.ipxsup && IPXInt2FHandler())
      return;
    break;
#endif
    }

  switch (HI(ax)) {
  case 0x11:              /* redirector call? */
    if (mfs_redirector())
    return;
    break;

  case 0x16:		/* misc PM/Win functions */
    if (!config.dpmi) {
/*  d.emu=4; */
      break;		/* fall into default_interrupt() */
    }
    switch (LO(ax)) {
      case 0x00:		/* WINDOWS ENHANCED MODE INSTALLATION CHECK */
#if 1			/* it seens this confuse winos2 */
    if (in_dpmi && in_win31) {
      D_printf("WIN: WINDOWS ENHANCED MODE INSTALLATION CHECK\n");
      LWORD(eax) = 0x0a03;	/* let's try enhaced mode 3.1 :-))))))) */
      return;
      }
#endif    
    break;

      case 0x05:		/* Win95 Initialization Notification */
    LWORD(ecx) = 0xffff;	/* say it`s NOT ok to run under Win */
      case 0x06:		/* Win95 Termination Notification */
      case 0x07:		/* Win95 Device CallOut */
      case 0x08:		/* Win95 Init Complete Notification */
      case 0x09:		/* Win95 Begin Exit Notification */
    return;

      case 0x0a:			/* IDENTIFY WINDOWS VERSION AND TYPE */
    if(in_dpmi && in_win31) {
      D_printf ("WIN: WINDOWS VERSION AND TYPE\n");
      LWORD(eax) =0;
      LWORD(ebx) = 0x030a;	/* 3.10 */
#if 1      
      LWORD(ecx) = 0x0003;	/* let\'s try enhaced mode */
#else
      LWORD(ecx) = 0x0002;	/* standard mode */
#endif      
      return;
        }
      break;

      case 0x83:
        if (in_dpmi && in_win31)
            LWORD (ebx) = 0;	/* W95: number of virtual machine */
      case 0x81:		/* W95: enter critical section */
        if (in_dpmi && in_win31) {
	    D_printf ("WIN: enter critical section\n");
	    /* LWORD(eax) = 0;	W95 DDK says no return value */
	    return;
  }
      break;
      case 0x82:		/* W95: exit critical section */
        if (in_dpmi && in_win31) {
	    D_printf ("WIN: exit critical section\n");
	    /* LWORD(eax) = 0;	W95 DDK says no return value */
	    return;
  }
        break;

      case 0x84:		/* Win95 Get Device Entry Point */
        LWORD(edi) = 0;
        WRITE_SEG_REG(es, 0);	/* say NO to Win95 ;-) */
        return;
      case 0x85:		/* Win95 Switch VM + Call Back */
        CARRY;
        LWORD(eax) = 1;
        return;

      case 0x86:            /* Are we in protected mode? */
        D_printf("DPMI CPU mode check in real mode.\n");
        if (in_dpmi) /* set AX to zero only if program executes in protected mode */
	    LWORD(eax) = 0;	/* say ok */
		 /* else let AX untouched (non-zero) */
      return;

      case 0x87:            /* Call for getting DPMI entry point */
	dpmi_get_entry_point();
	return;
    }
    break;

  case INT2F_XMS_MAGIC:
    if (!config.xms_size)
      break;
    switch (LO(ax)) {
    case 0:			/* check for XMS */
      x_printf("Check for XMS\n");
      xms_grab_int15 = 0;
      LO(ax) = 0x80;
      break;
    case 0x10:
      x_printf("Get XMSControl address\n");
      /* REG(es) = XMSControl_SEG; */
      WRITE_SEG_REG(es, XMSControl_SEG);
      LWORD(ebx) = XMSControl_OFF;
      break;
    default:
      x_printf("BAD int 0x2f XMS function:0x%02x\n", LO(ax));
    }
    return;
  }

  if (IS_REDIRECTED(i))
    default_interrupt(i);
}

/* mouse */
static void int33(u_char i) {
/* New code introduced by Ed Sirett (ed@cityscape.co.uk)  26/1/95 to give 
 * garrot control when the dos app is polling the mouse and the mouse is 
 * taking a break. */
  
  static unsigned short int oldx=0, oldy=0, trigger=0;
   
/* Firstly do the actual mouse function. */   
/* N.B. This code only works with the intdrv since default_interrupt() does not
 * actually call the real mode mouse driver now. (It simply sets up the registers so
 * that when the signal that we are currently handling has completed and the kernel
 * reschedules dosemu it will then start executing the real mode mouse handler). :-( 
 * Do we need/have we got post_interrupt (IRET) handlers? 
 */
#ifdef USE_NEW_INT
/* We have post_interrupt handlers in dpmi --EB 28 Oct 1997 */
#endif /* USE_NEW_INT */
  if (mice->intdrv) 
    mouse_int();
  else 
    default_interrupt(i);
/* It seems that the only mouse sub-function that could be plausibly used to 
 * poll the mouse is AX=3 - get mouse buttons and position. 
 * The mouse driver should have left AX=3 unaltered during its call.
 * The correct response should have the buttons in the low 3 bits in BX and 
 * x,y in CX,DX. 
 * Some programs seem to interleave calls to read mouse with various other 
 * sub-functions (Esp. 0x0b  0x05 and 0x06)
 * As a result we do not reset the  trigger value in these cases. 
 * Sadly, some programs use the user-specified mouse-event handler function (0x0c)
 * after which they then wait for mouse events presumably in a tight loop, I think
 * that we won't be able to stop these programs from burning CPU cycles.
 */
   if (LWORD(eax) ==0x0003)  {
     if (LWORD(ebx) == 0 && oldx == LWORD(ecx) && oldy == LWORD(edx) ) 
        trigger++;
      else  { 
        trigger=0;
        oldx = LWORD(ecx);
        oldy = LWORD(edx);
      } 
   }
m_printf("Called/ing the mouse with AX=%x \n",LWORD(eax));
/* Ok now we test to see if the mouse has been taking a break and we can let the 
 * system get on with some real work. :-) */
   if (config.hogthreshold && trigger >= config.hogthreshold)  {
     m_printf("Ignoring the quiet mouse.\n");
     usleep(INT2F_IDLE_USECS);
     trigger=0;
   }
}

#ifdef USING_NET
/* new packet driver interface */
static void int_pktdrvr(u_char i) {
  if (!pkt_int())
    default_interrupt(i);
}
#endif

/* dos helper and mfs startup (was 0xfe) */
static void inte6(u_char i) {
  if (!dos_helper())
    default_interrupt(i);
}

/* mfs FCB call */
static void inte7(u_char i) {
  SETIVEC(0xe7, INTE7_SEG, INTE7_OFF);
  real_run_int(0xe7);
}

#ifdef USE_INT_QUEUE
/* End function for interrupt calls from int_queue_run() */
static void inte8(u_char i) {
  static unsigned short *csp;
  static int x;
  csp = SEG_ADR((us *), cs, ip) - 1;

  for (x=1; x<=int_queue_running; x++) {
    /* check if any int is finished */
    if ((int)csp == int_queue_head[x].int_queue_return_addr) {
      /* if it's finished - clean up by calling user cleanup fcn. */
      if (int_queue_head[x].int_queue_ptr.callend) {
	int_queue_head[x].int_queue_ptr.callend(int_queue_head[x].int_queue_ptr.interrupt);
	int_queue_head[x].int_queue_ptr.callend = NULL;
      }

      /* restore registers */
      REGS = int_queue_head[x].saved_regs;
      /* REG(eflags) |= VIF; */
      WRITE_FLAGS(READ_FLAGS() | VIF);
      
      h_printf("e8 int_queue: finished %x\n",
	       int_queue_head[x].int_queue_return_addr);
      *OUTB_ADD=1;
      if (int_queue_running == x) 
	int_queue_running--;
      return;
    }
  }
  h_printf("e8 int_queue: shouldn't get here\n");
  show_regs(__FILE__,__LINE__);
}
#endif /* USE_INT_QUEUE */

/*
 * DANG_BEGIN_FUNCTION DO_INT 
 *
 * description:
 * DO_INT is used to deal with interrupts returned to DOSEMU by the
 * kernel.
 *
 * DANG_END_FUNCTION
 */

void do_int(int i)
{
#ifndef USE_NEW_INT
  void (* caller_function)();

  if ((LWORD(cs) != BIOSSEG) && IS_REDIRECTED(i) && can_revector(i)){
    real_run_int(i);
    return;
  }

  caller_function = interrupt_function[i];
  caller_function(i);
  /* This is a kludge to avoid immediate respawning of dosemu
   * if we have cs:ip pointing to the BIOS stubs' IRET
   * We do the IRET ourselves, so we directly let the DOSapp
   * control.
   * This perhaps avoids chained timer ints to accumulate
   * without giving the DOSapp a chance to do something.
   * (not sure if it works)
   * - Hans Lermen
   */ 
  if (LWORD(cs) == BIOSSEG) {
    unsigned char *csp;
    csp = SEG_ADR((unsigned char *),cs,ip);
    if ((*csp == 0xcf)) {
      unsigned char *ssp = (unsigned char *)(LWORD(ss)<<4);
      unsigned long sp = (unsigned long) LWORD(esp);
      LWORD(esp) +=6;
      LWORD(eip) = popw(ssp, sp);
      REG(cs) = popw(ssp, sp);
      WRITE_FLAGS(popw(ssp, sp));
      if (READ_FLAGS() & IF) WRITE_FLAGSE(READ_FLAGSE() | VIF);
      else  WRITE_FLAGSE((READ_FLAGSE() | IF) & ~VIF);
    }
  }
#else /* USE_NEW_INT */
 	unsigned long magic_address;
	if (in_dpmi) {
		if (dpmi_eflags & IF) {
			set_IF();
		} else {
			clear_IF();
		}
	}
	
 	if ((d.defint > 2) && (((i != 0x28) && (i != 0x2f)) || in_dpmi)) {
 		di_printf("Do INT0x%02x eax=0x%08x ebx=0x%08x ss=0x%08x esp=0x%08x\n"
 			  "           ecx=0x%08x edx=0x%08x ds=0x%08x  cs=0x%08x ip=0x%08x\n"
 			  "           esi=0x%08x edi=0x%08x es=0x%08x flg=0x%08x\n",
 			  i, _EAX, _EBX, _SS, _ESP,
 			  _ECX, _EDX, _DS, _CS, _IP,
 			  _ESI, _EDI, _ES, (int) read_EFLAGS());
 	}
	
#if 1  /* This test really ought to be in the main loop before
 	*  instruction execution not here. --EB 10 March 1997 
 	*/
	
 	/* try to catch jumps to 0:0 (e.g. uninitialized user interrupt vectors),
 	   which sometimes can crash the whole system, not only dosemu... */
 	if (SEGOFF2LINEAR(_CS, _IP) < 1024) {
 		dbug_printf("OUCH! attempt to execute interrupt table - quickly dying\n");
 		leavedos(57);
 	}
#endif
  
  
 	/* see if I want to use the caller function */
 	/* I want to use it if I must always use it, or I am calling into the
 	   interrupt table at the start of the dosemu bios */
 	/* assume IP was just incremented by 2 past int int instruction which set us
 	   off */
	
 	magic_address = SEGOFF2LINEAR(BIOSSEG, INT_OFF(i));
 	if (magic_address == (SEGOFF2LINEAR(_CS, _IP) -2)) {
 		run_caller_func(i, FALSE);
 	}
 	else if ((magic_address == IVEC(i)) ||
 		 (can_revector(i) == REVECT)) {
 		run_caller_func(i, TRUE);
 	}
 	else {
 		real_run_int(i);
 	}
#endif /* USE_NEW_INT */
}


#ifdef USE_INT_QUEUE
/*
 * Called to queue a hardware interrupt - will call "callstart"
 * just before the interrupt occurs and "callend" when it finishes
 */
void queue_hard_int(int i, void (*callstart), void (*callend))
{
  cli();

  int_queue[int_queue_end].interrupt = i;
  int_queue[int_queue_end].callstart = callstart;
  int_queue[int_queue_end].callend = callend;
  int_queue_end = (int_queue_end + 1) % IQUEUE_LEN;

  h_printf("int_queue: (%d,%d) ", int_queue_start, int_queue_end);

  i = int_queue_start;
  while (i != int_queue_end) {
    h_printf("%x ", int_queue[i].interrupt);
    i = (i + 1) % IQUEUE_LEN;
  }
  h_printf("\n");

  if (int_queue_start == int_queue_end)
    leavedos(56);
  sti();
}

/* Called by vm86() loop to handle queueing of interrupts */
void int_queue_run(void)
{
  static int current_interrupt;
  static unsigned char *ssp;
  static unsigned long sp;
  static u_char vif_counter=0; /* Incase someone don't clear things */

  if (int_queue_start == int_queue_end) {
#if 0
    REG(eflags) &= ~(VIP);
#endif
    return;
  }

  g_printf("Still using int_queue_run()\n");

  if (!*OUTB_ADD) {
    if (++vif_counter > 0x08) {
      I_printf("OUTB interrupts renabled after %d attempts\n", vif_counter);
    }
    else {
      REG(eflags) |= VIP;
      I_printf("OUTB_ADD = %d , returning from int_queue_run()\n", *OUTB_ADD);
      return;
    }
  }

  if (!(REG(eflags) & VIF)) {
    if (++vif_counter > 0x08) {
      I_printf("VIF interrupts renabled after %d attempts\n", vif_counter);
    }
    else {
      REG(eflags) |= VIP;
      I_printf("interrupts disabled while int_queue_run()\n");
      return;
    }
  }

  vif_counter=0;
  current_interrupt = int_queue[int_queue_start].interrupt;

  ssp = (unsigned char *) (REG(ss) << 4);
  sp = (unsigned long) LWORD(esp);

   
  /* call user startup function...don't run interrupt if returns -1 */
  if (int_queue[int_queue_start].callstart) {
    if (int_queue[int_queue_start].callstart(current_interrupt) == -1) {
      fprintf(stderr, "Callstart NOWORK\n");
      return;
    }

    if (int_queue_running + 1 == NUM_INT_QUEUE)
      leavedos(55);

    int_queue_head[++int_queue_running].int_queue_ptr = int_queue[int_queue_start];
    int_queue_head[int_queue_running].in_use = 1;

    /* save our regs */
    int_queue_head[int_queue_running].saved_regs = REGS;

    /* push an illegal instruction onto the stack */
    /*  pushw(ssp, sp, 0xffff); */
    pushw(ssp, sp, 0xe8cd);

    /* this is where we're going to return to */
    int_queue_head[int_queue_running].int_queue_return_addr = (unsigned long) ssp + sp;
    pushw(ssp, sp, vflags);
    /* the code segment of our illegal opcode */
    pushw(ssp, sp, int_queue_head[int_queue_running].int_queue_return_addr >> 4);
    /* and the instruction pointer */
    pushw(ssp, sp, int_queue_head[int_queue_running].int_queue_return_addr & 0xf);
    LWORD(esp) -= 8;
  } else {
    pushw(ssp, sp, vflags);
    /* the code segment of our iret */
    pushw(ssp, sp, LWORD(cs));
    /* and the instruction pointer */
    pushw(ssp, sp, LWORD(eip));
    LWORD(esp) -= 6;
  }

  if (current_interrupt < 0x10)
    *OUTB_ADD = 0;
  else
    *OUTB_ADD = 1;

  LWORD(cs) = ((us *) 0)[(current_interrupt << 1) + 1];
  LWORD(eip) = ((us *) 0)[current_interrupt << 1];

  /* clear TF (trap flag, singlestep), IF (interrupt flag), and
   * NT (nested task) bits of EFLAGS
   */
#if 0
  REG(eflags) &= ~(VIF | TF | IF | NT);
#endif
  if (int_queue[int_queue_start].callstart)
    REG(eflags) |= VIP;

  int_queue_start = (int_queue_start + 1) % IQUEUE_LEN;
  h_printf("int_queue: running int %x if applicable, return_addr=%x\n",
	   current_interrupt,
	   int_queue_head[int_queue_running].int_queue_return_addr);
}
#endif /* USE_INT_QUEUE */

/*
 * DANG_BEGIN_FUNCTION setup_interrupts
 *
 * description:
 * SETUP_INTERRUPTS is used to initialize the interrupt_function
 * array which directs handling of interrupts in protected mode and
 * also initializes the base vector for interrupts in real mode.
 *
 * DANG_END_FUNCTION
 */

void setup_interrupts(void) {
  int i;
  unsigned char *ptr;
  ushort *seg, *off;

  /* init trapped interrupts called via jump */
  for (i = 0; i < 256; i++) {
    interrupt_function[i] = default_interrupt;
    if ((i & 0xf8) == 0x60) { /* user interrupts */
	/* show also EMS (int0x67) as disabled */
	SETIVEC(i, 0, 0);
    } else {
#ifndef USE_NEW_INT
	SETIVEC(i, BIOSSEG, 16 * i);
#else /* USE_NEW_INT */
	SETIVEC(i, BIOSSEG, INT_OFF(i));
#endif /* USE_NEW_INT */
    }
  }
  
  interrupt_function[5] = int05;
#ifndef USE_NEW_INT
  interrupt_function[8] = int08;
  interrupt_function[9] = int09;
  interrupt_function[0xa] = int_a_b_c_d_e_f;
  interrupt_function[0xb] = int_a_b_c_d_e_f;
  interrupt_function[0xc] = int_a_b_c_d_e_f;
  interrupt_function[0xd] = int_a_b_c_d_e_f;
  interrupt_function[0xe] = int_a_b_c_d_e_f;
  interrupt_function[0xf] = int_a_b_c_d_e_f;
#endif /* not USE_NEW_INT */
  /* This is called only when revectoring int10 */
  interrupt_function[0x10] = int10;
  interrupt_function[0x11] = int11;
  interrupt_function[0x12] = int12;
  interrupt_function[0x13] = int13;
  interrupt_function[0x14] = int14;
  interrupt_function[0x15] = int15;
#ifndef USE_NEW_INT
  interrupt_function[0x16] = int16;
#endif /* not USE_NEW_INT */
  interrupt_function[0x17] = int17;
  interrupt_function[0x18] = int18;
  interrupt_function[0x19] = int19;
  interrupt_function[0x1a] = int1a;
  interrupt_function[0x21] = int21;
#ifdef DJGPP_HACK
  interrupt_function[0x23] = int23;
#endif
  interrupt_function[0x28] = int28;
  interrupt_function[0x29] = int29;
  interrupt_function[0x2f] = int2f;
  interrupt_function[0x33] = int33;
#ifdef USING_NET
  interrupt_function[0x60] = int_pktdrvr;
#endif
  interrupt_function[0xe6] = inte6;
  interrupt_function[0xe7] = inte7;
#ifdef USE_INT_QUEUE
  interrupt_function[0xe8] = inte8;
#endif

  /* Let kernel handle this, no need to return to DOSEMU */
#ifndef USE_NEW_INT
  SETIVEC(0x1c, 0xf010, 0xc0);
#else /* USE_NEW_INT */
 #if 0
  SETIVEC(0x1c, BIOSSEG + 0x10, INT_OFF(0x1c) +2 - 0x100);
 #endif
#endif /* USE_NEW_INT */

  /* show EMS as disabled */
  SETIVEC(0x67, 0, 0);

  if (mice->intdrv) {
    /* this is the mouse handler */
    ptr = (unsigned char *) (Mouse_ADD+12);
    off = (u_short *) (Mouse_ADD + 8);
    seg = (u_short *) (Mouse_ADD + 10);
    /* tell the mouse driver where we are...exec add, seg, offset */
    mouse_sethandler(ptr, seg, off);
#ifndef USE_NEW_INT
    SETIVEC(0x74, Mouse_SEG, Mouse_ROUTINE_OFF);
    SETIVEC(0x33, Mouse_SEG, Mouse_ROUTINE_OFF);
  }
  else
    *(unsigned char *) (BIOSSEG * 16 + 16 * 0x33) = 0xcf;	/* IRET */
#else /* USE_NEW_INT */
  }
  else {
    /* point to the retf#2 immediately after the int 33 entry point. */
    SETIVEC(0x33, BIOSSEG, INT_OFF(0x33)+2);
  }
#endif /* USE_NEW_INT */

  SETIVEC(0x16, INT16_SEG, INT16_OFF);
  SETIVEC(0x09, INT09_SEG, INT09_OFF);
  SETIVEC(0x08, INT08_SEG, INT08_OFF);
  SETIVEC(0x70, INT70_SEG, INT70_OFF);

  /* Install new handler for video-interrupt into bios_f000_int10ptr,
   * for video initialization at f800:4200
   * If config_vbios_seg=0xe000 -> e000:3, else c000:3
   * Next will be the call to int0xe6,al=8 which starts video BIOS init
   */
  install_int_10_handler();

  /* This is an int e7 used for FCB opens */
  SETIVEC(0xe7, INTE7_SEG, INTE7_OFF);
  /* End of int 0xe7 for FCB opens */

  /* set up relocated video handler (interrupt 0x42) */
  if (config.dualmon == 2) {
    interrupt_function[0x42] = interrupt_function[0x10];
  }
#ifndef USE_NEW_INT
  else *(u_char *) 0xff065 = 0xcf;	/* IRET */
#endif /* not USE_NEW_INT */
}


void set_int21_revectored(int a)
{
  static int rv_all = 0;
  int i;

  ds_printf("INT21: rv_all: %d + %d = ", rv_all, a);

  rv_all += a;

  if(rv_all > 0) {
    memset(&vm86s.int21_revectored, 0xff, sizeof(vm86s.int21_revectored));
  }
  else {
    memset(&vm86s.int21_revectored, 0x00, sizeof(vm86s.int21_revectored));
    for(i = 0; i < 0x100; i++)
      if(can_revector_int21(i) == REVECT) set_revectored(i, &vm86s.int21_revectored);
  }

  ds_printf("%d\n", rv_all);
}


/*
 * DANG_BEGIN_FUNCTION int_vector_setup
 *
 * description:
 * Setup initial interrupts which can be revectored so that the kernel
 * does not need to return to DOSEMU if such an interrupt occurs.
 *
 * DANG_END_FUNCTION
 */

void int_vector_setup(void)
{
  int i;

  /* set up the redirection arrays */
#ifdef __linux__
  memset(&vm86s.int_revectored, 0x00, sizeof(vm86s.int_revectored));
  memset(&vm86s.int21_revectored, 0x00, sizeof(vm86s.int21_revectored));

  for (i=0; i<0x100; i++)
    if (can_revector(i)==REVECT && i!=0x21)
      set_revectored(i, &vm86s.int_revectored);

  set_int21_revectored(0);
#endif

}

#ifdef USE_MRP_JOYSTICK
void mrp_read_joystick(void)
{
  /* This only reads the first joystick and doesn't handle the buttons. */
  /* You will need to install the joystick module for this to work. */
  static int fd =0;
  static const char * const fname = "/dev/js0" ;
  int status;
  struct JS_DATA_TYPE js;

  if (fd == 0) {
    fd = open (fname, O_RDONLY);
    if (fd < 0)
      perror ("mrp_read_joystick (open)");
  }
  if (fd < 0) {
    CARRY; /* fail */
    return;
  }

  status = read (fd, &js, JS_RETURN);
  if (status != JS_RETURN) {
    perror ("mrp_read_joystick (read)");
    fd = -1;
    CARRY; /* fail */
    return;
  }
  LWORD(eax) = js.x;
  LWORD(ebx) = js.y;
  LWORD(ecx) = 0;		/* second joystick x */
  LWORD(edx) = 0;		/* second joystick y */
  NOCARRY;
  return;
}
#endif
