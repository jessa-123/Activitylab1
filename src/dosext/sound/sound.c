/* 
 * (C) Copyright 1992, ..., 2002 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * DANG_BEGIN_MODULE
 *
 * Description: SB emulation - Getting There ...!
 * 
 * maintainer: 
 *   Alistair MacDonald <alistair@slitesys.demon.co.uk>
 *   Stas Sergeev <stssppnn@yahoo.com>
 *
 * DANG_END_MODULE
 *
 * Previous Maintainers:
 *  Joel N. Weber II
 *  Michael Beck
 *  David Brauman <crisk@netvision.net.il>
 *  Rutger Nijlunsing <rutger@null.net>
 *  Michael Karcher <Michael.Karcher@writeme.com>
 *
 * Key:
 *  AM/Alistair - Alistair MacDonald
 *  CRISK       - David Brauman
 *  MK/Karcher  - Michael Karcher
 *
 * History: (AM, unless noted)
 * ========
 * The original code was written by Joel N. Weber II. See README.sound
 * for more information. I (Alistair MacDonald) made the code compile, and
 * added a few extra features. I took the code and continued development on
 * 0.61. Michael Beck did a lot of work on 0.60.4, including capability
 * detection under Linux. This code is basically
 * my 0.61 code, brought back into mainstream DOSEmu (0.63), but with
 * Michael's code where I thought it was better than mine. I also separated
 * out the Linux specific driver code. - Alistair
 *
 * 0.67 has seen the introduction of stub code for handling Adlib (the timers
 * work after a fashion now) and changes to handle auto-init DMA. I've merged
 * some code from Michael Karcher (Michael.Karcher@writeme.com) although I
 * can't use all of it because it duplicates the auto-init (and I prefer my 
 * way - its cleaner!
 *
 * Included Michael's reworked Auto-Init, as it fixed a number of problems with
 * my version. (His new version _is_ cleaner than it was!)
 *
 * [and I prefer my way - it works! - MK]
 *
 *
 * Rewrote the code almost completely to make it actually work.
 * Many thanks to Vlad Romascanu for the usefull info and hints.
 * -- Stas Sergeev
 *
 * 
 *
 * Original Copyright Notice:
 * ==========================
 * Copyright 1995  Joel N. Weber II
 * See the file README.sound in this directory for more information 
 */

/* Uncomment following to force complete emulation of some varient of
 * Creative Technology's SB sound card
 *
 * This should only be used to experiment with some of the undocumented
 * features that are used by Creative Technology's utilities.  Among
 * other things it changes the response to the E3 copyright message
 * request to match the real SB hardware copyright message.
 * It should match your REAL card.
 */
/* #define STRICT_SB_EMU SB_AWE32 */


#include "emu.h"
#include "iodev.h"
#include "int.h"
#include "port.h"
#include "dma.h"
#include "timers.h"
#include "sound.h"
#include "pic.h"
#include "bitops.h"
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

/* Internal Functions */

inline void dsp_write_output(__u8 value);
inline void dsp_clear_output(void);
inline __u8 dsp_read_output(void);

size_t sb_dma_read(void *ptr, size_t size);
size_t sb_dma_write(void *ptr, size_t size);

static void sb_dma_start(void);
static void start_dsp_dma(void);
static void restart_dsp_dma(void);
static void pause_dsp_dma(void);

void sb_irq_trigger (void);

static void sb_enable_speaker (void);
static void sb_disable_speaker(void);
static void sb_write_DAC (int bits, __u8 value);

static void dsp_do_copyright(void);

static __u8 sb_read_mixer(__u8 ch);
static void sb_write_mixer(int ch, __u8 value);

/* DANG_FIXTHIS The file header needs tidying up a _LOT_ ! */

static void sb_init(void);
static void fm_init(void);
static void mpu401_init(void);

static void sb_reset(void);
static void sb_detect(void);
static void fm_reset(void);
static void mpu401_reset(void);

static void sb_dsp_write ( Bit8u value );

void sb_do_sine (void);
void sb_get_aux_status (void);
void sb_get_version (void);
void sb_dsp_get_status (void);
void sb_dsp_unsupported_command (void);
void sb_write_silence (void);
void sb_do_midi_write (void);

void sb_cms_write (ioport_t port, Bit8u value);

inline void sb_mixer_register_write (Bit8u value);
void sb_mixer_data_write (Bit8u value);
Bit8u sb_mixer_data_read (void);

void sb_do_reset (Bit8u value);

void sb_update_timers (void);

static void sb_activate_irq (int type);
static void sb_deactivate_irq (int type);
static void sb_check_complete (void);

Bit8u sb_get_mixer_IRQ_mask (void);
Bit8u sb_irq_to_bit (Bit8u irq);
Bit8u sb_get_mixer_DMA_mask (void);
Bit8u sb_get_mixer_IRQ_status (void);

static int dsp_block_size;
static int dma_pending = 0;
static int mixer_emu_regs[255];

static int into_irq = 0;
static int DSP_busy_hack = 0;
static int byte_skipped = 0;
static char m_E2Value = 0xaa;
static int m_E2Count = 0;
static int E2_Active = 0;

int E2_incr_table[4][9] = {
  {  0x01, -0x02, -0x04,  0x08, -0x10,  0x20,  0x40, -0x80, -106 },
  { -0x01,  0x02, -0x04,  0x08,  0x10, -0x20,  0x40, -0x80,  165 },
  { -0x01,  0x02,  0x04, -0x08,  0x10, -0x20, -0x40,  0x80, -151 },
  {  0x01, -0x02,  0x04, -0x08, -0x10,  0x20, -0x40,  0x80,   90 }
};

/*
 ************************************************************
 * DSP Queue functions : Used to allow multi-byte responses *
 ************************************************************
 */

void dsp_write_output(__u8 value)
{
  /* There is no check for exceeding the length of the queue ... */

  ++SB_queue.holds;
  SB_queue.output[SB_queue.end++] = value;
  SB_queue.end &= (DSP_QUEUE_SIZE - 1);
  SB_dsp.data = SB_DATA_AVAIL;

  if (debug_level('S') >= 2) {
    S_printf ("SB: Insert into output Queue [%u]... (0x%x)\n", 
	    SB_queue.holds, value);
  }
}

void dsp_clear_output(void)
{
  if (debug_level('S') >= 2) {
    S_printf ("SB: Clearing the output Queue\n");
  }

  SB_queue.holds = 0;
  SB_queue.end   = 0;
  SB_queue.start = 0;
  SB_dsp.data    = SB_DATA_UNAVAIL;
}

__u8 dsp_read_output(void)
{
  Bit8u r = 0xFF;

  if (SB_queue.holds) {
    r = SB_queue.output[SB_queue.start];
    SB_queue.output[SB_queue.start++] = 0x00;
    SB_queue.start &= (DSP_QUEUE_SIZE - 1);

    if (--SB_queue.holds)
      SB_dsp.data = SB_DATA_AVAIL;
    else
      SB_dsp.data = SB_DATA_UNAVAIL;

    if (debug_level('S') >= 2) {
      S_printf ("SB: Remove from output Queue [%u]... (0x%X)\n", 
	      SB_queue.holds, r);
    }
  }
  return r;
}


/*
 * Main IO Routines - Read
 * =======================
 */

/*
 * DANG_BEGIN_FUNCTION sb_io_read
 *
 * arguments:
 * port - The I/O port being read from.
 *
 * description:
 * This handles all of the reads for the SB emulation. The value read is
 * returned. The value of 0xFF indicates an invalid read. (assumes the ports
 * float high when not pulled low by the hardware.)
 *
 * DANG_END_FUNCTION
 */

Bit8u sb_io_read(ioport_t port)
{
  ioport_t addr;
  __u8 value;
  Bit8u result;

  if (SB_info.version == SB_NONE)
    return 0xff;

  addr = port - config.sb_base;

  switch (addr) {

    /* == FM Music == */

   case 0x00:
	/* FM Music Left Status Port - SBPro */
	if (SB_info.version >= SB_PRO) {
		result = fm_io_read (FM_LEFT_STATUS);
	} else {
		result = 0xFF;
	}
	break;

   case 0x02:
	/* FM Music Right Status Port - SBPro */
	if (SB_info.version >= SB_PRO) {
		result = fm_io_read (FM_RIGHT_STATUS);
	}
	else {
		result = 0xFF;
	}
	break;
    
   case 0x05: /* Mixer Data Register */
     result = sb_mixer_data_read();
		break;

   case 0x06: /* Reset ? */
     S_printf("SB: read from Reset address\n");
     result = 0; /* Some programms read this whilst resetting */
     break;

   case 0x08:
	/* FM Music Compatible Status Port - SB */
	/* (Alias to 0x00 on SBPro) */
	result = fm_io_read(FM_LEFT_STATUS);
	break;

   case 0x0A: /* DSP Read Data - SB */
     value = dsp_read_output(); 
     S_printf ("SB: Read 0x%x from SB DSP\n", value);
     result = value;
     break;

   case 0x0C: /* DSP Write Buffer Status */
     result = SB_dsp.ready;
     if (SB_dsp.ready)
       SB_dsp.ready--;		/* HACK */
     if(DSP_busy_hack == 1)
       result = 0x80;
     if(DSP_busy_hack)
       DSP_busy_hack--;
     S_printf ("SB: Write? %x\n", result);
     break;

   case 0x0D: /* DSP MIDI Interrupt Clear - SB16 ? */
     S_printf("SB: read 16-bit MIDI interrupt status. Not Implemented.\n");
     SB_info.irq.active &= ~SB_IRQ_MIDI; /* may mean it never triggers! */
     result = 0xFF;
     break;

   case 0x0E:		
     /* DSP Data Available Status - SB */
     /* DSP 8-bit IRQ Ack - SB */
     S_printf("SB: 8-bit IRQ Ack: %x\n", SB_dsp.data);
     sb_deactivate_irq(SB_IRQ_8BIT);
     SB_info.irq.active &= ~SB_IRQ_8BIT; /* may mean it never triggers! */
     SB_dsp.ready = 0x7f;
     if(SB_dsp.empty_state & DREQ_AT_EOI)
     {
       if(!SB_dsp.pause_state)
         dma_assert_DREQ(config.sb_dma);
       SB_dsp.empty_state &= ~DREQ_AT_EOI;
     }
     result = SB_dsp.data;
     break;

   case 0x0F: /* 0x0F: DSP 16-bit IRQ - SB16 */
     S_printf("SB: 16-bit IRQ Ack: %x\n", SB_dsp.data);
     sb_deactivate_irq(SB_IRQ_16BIT);
     SB_info.irq.active &= ~SB_IRQ_16BIT; /* may mean it never triggers! */
     SB_dsp.ready = 0x7f;
     if(SB_dsp.empty_state & DREQ_AT_EOI)
     {
       if(!SB_dsp.pause_state)
         dma_assert_DREQ(config.sb_dma);
       SB_dsp.empty_state &= ~DREQ_AT_EOI;
     }
     result = SB_dsp.data;
     break;

     /* == CD-ROM - UNIMPLEMENTED == */

   case 0x10: /* CD-ROM Data Register - SBPro */
     if (SB_info.version > SB_PRO) {
       S_printf("SB: read from CD-ROM Data register.\n");
       result = 0;
     }
     else {
       S_printf("SB: CD-ROM not supported in this version.\n");
       result = 0xFF;
     }
		break;

   case 0x11: /* CD-ROM Status Port - SBPro */
     if (SB_info.version > SB_PRO) {
       S_printf("SB: read from CD-ROM status port.\n");
       result = 0xFE;
     }
     else {
       S_printf("SB: CD-ROM not supported in this version.\n");
       result = 0xFF;
     }
		break;

   default:
	S_printf("SB: %#x is an unhandled read port.\n", port);
		result = 0xFF;
	};

  if (debug_level('S')>= 3) {
    S_printf ("SB: port read 0x%x returns 0x%x\n", port, result);
  }

  return result;
}

Bit8u sb_mixer_data_read (void)
{
  int value;
  /* DANG_FIXME Only CT1345 supported */

	if (SB_info.version > SB_20) {
	    switch (SB_info.mixer_index) {
		case 0x04:
			value = sb_read_mixer(SB_MIXER_PCM);
			break;
		case 0x0A:
			value = sb_read_mixer(SB_MIXER_MIC);
			break;

		case 0x0C:
			value = 32;	/* filters not supported */
			break;

		case 0x0E:
			value = (SB_dsp.stereo ? 2 : 0) | 32;
			break;

		case 0x22:
			value = sb_read_mixer(SB_MIXER_VOLUME);
			break;

		case 0x26:
			value = sb_read_mixer(SB_MIXER_SYNTH);
			break;

		case 0x28:
			value = sb_read_mixer(SB_MIXER_CD);
			break;

		case 0x2E:
			value = sb_read_mixer(SB_MIXER_LINE);
			break;

			/* === SB16 Registers === */

			/* 
			 * Additional registers, originally from Karcher
			 * Updated to remove assumptions and separate into 
			 * functions - AM 
			 */

	        case 0x80: /* IRQ Select */
		        return sb_get_mixer_IRQ_mask();

		case 0x81: /* DMA Select */
		        return sb_get_mixer_DMA_mask();

	        case 0x82: /* IRQ Status */
		        return sb_get_mixer_IRQ_status();
		
		default:
			S_printf("SB: invalid read from mixer (%x)\n", 
				 SB_info.mixer_index);
			value = 0xFF;
			break;
	    }
	    if (value != mixer_emu_regs[SB_info.mixer_index] &&
	      mixer_emu_regs[SB_info.mixer_index] > 0) {
	      S_printf("SB: Emulated (0x%x) and real (0x%x) values mismatch for Mixer register 0x%x\n",
	      mixer_emu_regs[SB_info.mixer_index], value, SB_info.mixer_index);
	      value = mixer_emu_regs[SB_info.mixer_index];
	    }
	    return value;
	} else {
		S_printf("SB: mixer not supported on this SB version.\n");
		return 0xFF;
	}
}


Bit8u sb_get_mixer_IRQ_mask (void)
{
  Bit8u value;

  value = 0xF0; /* Reserved top bits are 1 */
  
  value |= sb_irq_to_bit (config.sb_irq);
  /* And for the other IRQs ... */

  return value;
}

Bit8u sb_irq_to_bit (Bit8u irq)
{
  switch (irq) {
  case 2:
    return 1;
  case 5:
    return 2;
  case 7:
    return 4;
  case 10:
    return 8;
  default:
    break;
  }

  return 0;
}


Bit8u sb_get_mixer_DMA_mask (void)
{
  Bit8u value;

  value = 0;

  /* 8-bit DMA */
  value |= (1 << config.sb_dma);

  /* and others .... */

  return value;
}

Bit8u sb_get_mixer_IRQ_status (void)
{
  Bit8u value;

  value = 0;

  value |= SB_info.irq.active;

  /* DSP V4 - minor version identifier */
  if (SB_info.version == SB_16) {
    value += 16;
  } else if (SB_info.version == SB_AWE32) {
    value += 128;
  }

  return value;
}


/*
 * DANG_BEGIN_FUNCTION adlib_io_read
 *
 * arguments:
 * port - The I/O port being read from.
 *
 * description:
 * This handles all of the reads for the adlib (FM) emulation. The value read 
 * is returned. The value of 0xFF indicates an invalid read. (assumes the ports
 * float high when not pulled low by the hardware.)
 * The FM emulation is not written yet. The current plan is to use the midi
 * emulation where available as this is the most common use for the FM sound.
 *
 * DANG_END_FUNCTION
 */

Bit8u adlib_io_read(ioport_t port)
{
  Bit8u result = 0xFF;

  /* Adlib Base Port is 0x388 */
  /* Adv. Adlib Base Port is 0x38A */
  
  switch (port){
  case 0x388:    
    S_printf ("Adlib: Read from Adlib port (%#x)\n", port);
    result = fm_io_read (ADLIB_STATUS);
    break;

  case 0x38A:    
    S_printf ("Adv_Adlib: Read from Adlib Advanced port (%#x)\n", port);
    result = fm_io_read (ADV_ADLIB_STATUS);
    break;

	default:
		S_printf("%#x is an unhandled read port\n", port);
  };
  
  if (debug_level('S') >= 2) {
    S_printf ("Adlib: Read from port 0x%x returns 0x%x\n", port, result);
  }

  return result;
}

Bit8u fm_io_read (ioport_t port)
{
  /* extern struct adlib_info_t adlib_info; - For reference - AM */
  extern struct adlib_timer_t adlib_timers[2];
  Bit8u retval;

  switch (port){
  case ADLIB_STATUS:
		/* DANG_FIXTHIS Adlib status reads are unimplemented */
    /* retval = 31; - according to sblast.doc ? */
    retval = 0; /* - according to adlib_sb.txt */
    if ( (adlib_timers[0].expired == 1) 
	 && (adlib_timers[0].enabled == 1) ) {
      retval |= (64 | 128);
    }
    if ( (adlib_timers[1].expired == 1) 
	 && (adlib_timers[1].enabled == 1) ) {
      retval |= (32 | 128) ;
    }
    S_printf ("Adlib: Status read - %d\n", retval);
    return retval;

  case ADV_ADLIB_STATUS:
		/* DANG_FIXTHIS Advanced adlib reads are unimplemented */
    return 31;
  };
  
  return 0;
}

/*
 * DANG_BEGIN_FUNCTION mpu401_io_read
 *
 * arguments:
 * port - The I/O port being read from.
 *
 * description:
 * The MPU-401 functionality is primarily provided by 'midid' - a standalone
 * program. This makes most of the MPU-401 code simply a pass-through driver.
 *
 * DANG_END_FUNCTION
 */

Bit8u mpu401_io_read(ioport_t port)
{
  ioport_t addr;
	Bit8u r=0xff;
  
  addr = port - config.mpu401_base;
  
  switch(addr) {
  case 0:
    /* Read data port */
    r=0xfe; /* Whatever happened before, send a MPU_ACK */
    mpu401_info.isdata=0;
    S_printf("MPU401: Read data port = 0x%02x\n",r);
    sb_deactivate_irq(SB_IRQ_MIDI);
    break;
  case 1:
     /* Read status port */
    /* 0x40=OUTPUT_AVAIL; 0x80=INPUT_AVAIL */
    r=0xff & (~0x40); /* Output is always possible */
    if (mpu401_info.isdata) r &= (~0x80);
    S_printf("MPU401: Read status port = 0x%02x\n",r);
  }
  return r;
}


/*
 * Main IO Routines - Write
 * ========================
 */

/*
 * DANG_BEGIN_FUNCTION sb_io_write
 *
 * arguments:
 * port - The I/O port being written to.
 * value - The value being output.
 *
 * description:
 * This handles the writes for the SB emulation. Very little of the processing
 * is performed in this function as it basically consists of a very large
 * switch() statement. The processing here is limited to trivial (1 line) items
 * and distinguishing between the different actions and responses that the
 * different revisions of the SB series give.
 *
 * DANG_END_FUNCTION
 */

void sb_io_write(ioport_t port, Bit8u value)
{
  ioport_t addr;
  
  addr = port - config.sb_base;

  if (debug_level('S') >= 2) {
    S_printf("SB: [crisk] port 0x%04x value 0x%02x\n", (Bit16u)port, value);
  }
  
  switch (addr) {
    
    /* == FM MUSIC or C/MS == */
    
  case 0x00:
    if (SB_info.version >= SB_PRO) {
			/* FM Music Left Register Port - SBPro */
      fm_io_write (FM_LEFT_REGISTER, value);
		} else {
			/* C/MS Data Port (1-6) - SB Only */
			sb_cms_write (CMS_LOWER_DATA, value);
    }
    break;

  case 0x01:
    if (SB_info.version >= SB_PRO) {
			/* FM Music Left Data Register - SBPro */
      fm_io_write (FM_LEFT_DATA, value);
		} else {
			/* C/MS Register Port (1-6) - SB Only */
			sb_cms_write (CMS_LOWER_REGISTER, value);
    }
    break;

  case 0x02:
    if (SB_info.version >= SB_PRO) {
			/* FM Music Right Register Port - SBPro */
      fm_io_write (FM_RIGHT_REGISTER, value);
    }
    else {
			/* C/MS Data Port (7-12) - SB Only */
			sb_cms_write (CMS_UPPER_DATA, value);
    }
    break;

  case 0x03:
    if (SB_info.version >= SB_PRO) {
			/* FM Music Right Data Register - SBPro */
      fm_io_write (FM_RIGHT_DATA, value);
    }
    else {
			/* C/MS Register Port (7-12) - SB Only */
			sb_cms_write (CMS_UPPER_REGISTER, value);
    }
    break;
    
    /* == MIXER == */
    
  case 0x04: /* Mixer Register Port - SBPro */
		sb_mixer_register_write (value);
    break;

  case 0x05: /* Mixer Data Register - SBPro */
		sb_mixer_data_write (value);
		break;

		/* == RESET == */

  case 0x06:		/* reset */
		sb_do_reset (value);
		break;

		/* == FM MUSIC == */

  case 0x08:		
		/* FM Music Register Port - SB */
		/* Alias for 0x00 - SBPro */
		fm_io_write (FM_LEFT_REGISTER, value);
		break;

  case 0x09:
		/* FM Music Data Port - SB */
		/* Alias for 0x01 - SBPro */
		fm_io_write (FM_LEFT_DATA, value);
		break;

		/* == DSP == */

  case 0x0C:		/* dsp write register */
		if (SB_dsp.dma_mode & HIGH_SPEED_DMA) {
		  S_printf("SB: Commands are not permitted in High-Speed DMA mode!\n");
		  break;
		}
		sb_dsp_write ( value );
		break;

		/* 0x0D: Timer Interrupt Clear - SB16 */
		/* 0x10 - 0x13: CD-ROM - SBPro ***IGNORED*** */

  default:
		S_printf("SB: %x is an unhandled write port\n", addr);
  };
}

/*
 * DANG_FIXTHIS CMS Writes are unimplemented.
 */
void sb_cms_write (ioport_t port, Bit8u value)
{
	switch (port) {
	case CMS_LOWER_DATA:
		S_printf("SB: Write 0x%x to C/MS 1-6 Data Port\n", value);
		break;

	case CMS_LOWER_REGISTER:
		S_printf("SB: Write 0x%x to C/MS 1-6 Register Port\n", value);
		break;

	case CMS_UPPER_DATA:
		S_printf("SB: Write 0x%x to C/MS 7-12 Data Port\n", value);
		break;

	case CMS_UPPER_REGISTER:
		S_printf("SB: Write 0x%x to C/MS 7-12 Register Port\n", value);
		break;

	default:
		S_printf("SB: Invalid write to C/MS (%#x, %#x)\n", value,
			 port);
	}
}

inline void sb_mixer_register_write (Bit8u value)
{
    if (SB_info.version > SB_20) {
		SB_info.mixer_index = value;
    }
}

void sb_mixer_data_write (Bit8u value)
{
  if (SB_info.version > SB_20) {
    switch (SB_info.mixer_index) {
      case 0:
	/* 0 is the reset register, but we'll ignore it */
			S_printf("SB: Mixer reset\n");
			break;

      case 0x04:
			sb_write_mixer(SB_MIXER_PCM, value);
			break;

      case 0x0A:
			sb_write_mixer(SB_MIXER_MIC, value);
			break;

      case 0x0C:
			/* 0x0C is ignored - sets record source and a filter */
			if (!(value & 32)) {
			  S_printf("SB: Warning: Input filter is not supported!\n");
			  value |= 32;
			}
			break;

      case 0x0E: 
	if (!(value & 32)) {
	  S_printf("SB: Warning: Output filter is not supported!\n");
	  value |= 32;
	}
	if (!SB_dsp.pause_state && (SB_dsp.dma_mode & SB_USES_DMA) &&
	  SB_dsp.bytes_left && SB_dsp.stereo != ((value & 2) ? 1 : 0)) {
	  S_printf("SB: Changing mode while DMA is running?!\n");
	  SB_dsp.dma_mode &= ~SB_USES_DMA;	/* stop DMA :( */
	  SB_dsp.empty_state = START_DMA_AT_EMPTY | DMA_CONTINUE;
	}
	SB_dsp.stereo = (value & 2) ? 1 : 0;
	S_printf ("SB: Mode set to %s\n", (SB_dsp.stereo ? "Stereo" : "Mono"));
	break;	
		
      case 0x22: 
	sb_write_mixer(SB_MIXER_VOLUME, value);
	break;

      case 0x26: 
	sb_write_mixer(SB_MIXER_SYNTH, value);
	break;

      case 0x28: 
	sb_write_mixer(SB_MIXER_CD, value);
	break;

      case 0x2E: 
	sb_write_mixer(SB_MIXER_LINE, value);
	break;

      default:
			S_printf ("SB: Unknown index 0x%x in Mixer Write\n",
				  SB_info.mixer_index);
      break;
    } 
    mixer_emu_regs[SB_info.mixer_index] = value;
  }
}
    
void sb_do_reset (Bit8u value)
{
  dma_drop_DREQ(config.sb_dma);

  if (SB_dsp.dma_mode & HIGH_SPEED_DMA) {
/* for High-Speed mode reset means only exit High-Speed */
    S_printf("SB: Reset called, exiting High-Speed DMA mode\n");
    SB_dsp.dma_mode &= ~(HIGH_SPEED_DMA | SB_USES_DMA);
    return;
  }

  sb_reset();

  if (SB_info.version != SB_NONE && value != 0xC6) {
		/* From CRISK: Undocumented */
    dsp_write_output(0xAA);
  }
}
    
static inline void dma_start()
{
  if (SB_dsp.dma_mode & SB_USES_DMA)
    SB_dsp.empty_state |= START_DMA_AT_EMPTY;
  dma_pending = 1;
  dma_assert_DREQ(config.sb_dma);
  if (!SB_info.speaker && !pic_icount) {
   /* 
    * it seems that when speaker is disabled, DMA transfer is running at
    * maximum speed, disregarding the sampling rate. So we are going to
    * start and complete it right now.
    */
    dma_run();
    sb_check_complete();
  }
}
    
/*
 * DANG_BEGIN_FUNCTION sb_dsp_write
 *
 * arguments:
 * value - The value being written to the DSP.
 *
 * description:
 * The SB DSP is a complete I/O system in itself controlled via a number of
 * data bytes. The number of bytes depends upon the function. The function
 * to be executed is determined by the first byte.
 * If there is no existing command then the command is stored. This then used
 * in the switch to identify the action to be taken. When the command has 
 * supplied all of its arguments, or failed, then the command storage is 
 * cleared. Each DSP function is responsible for clearing this itself.
 * Again, this function relies on other functions to do the real work, and
 * apart from storing details of the command and parameters is basically a
 * large switch statement.
 *
 * DANG_END_FUNCTION
 */
    
void sb_dsp_write ( Bit8u value ) 
{
#define REQ_PARAMS(i) if (SB_dsp.num_parameters < i) return
#define PAR_LSB_MSB(i) (SB_dsp.parameter[i] | (SB_dsp.parameter[i+1] << 8))
#define PAR_MSB_LSB(i) ((SB_dsp.parameter[i] << 8) | SB_dsp.parameter[i+1])

	/* 
	 * ALL commands set SB_dsp.command to SB_NO_DSP_COMMAND when they 
	 * complete 
	 */

      if (SB_dsp.command == SB_NO_DSP_COMMAND ||
       SB_dsp.num_parameters >= MAX_DSP_PARAMS) {
      	/* No command yet */
      	SB_dsp.command = value;
      	SB_dsp.num_parameters = 0;
	S_printf("SB: DSP command 0x%x accepted\n", value);
      }
      else {
      	SB_dsp.parameter[SB_dsp.num_parameters++] = value;
	S_printf("SB: DSP parameter 0x%x accepted for command 0x%x\n",
	  value, SB_dsp.command);
      }

      switch (SB_dsp.command) {
      /* == STATUS == */

		/* 0x03: ASP Status - SB16ASP */
	
	case 0x04:	
	if (SB_info.version >= SB_20 && SB_info.version <= SB_PRO) {
			/* DSP Status - SB2.0-Pro2 - Obselete */
			sb_dsp_get_status ();
		} else {
			/* ASP ? - SB16ASP */
			sb_dsp_unsupported_command ();
	}
	break;
	
		/* 0x05: ??? - SB16ASP */
	case 0x05:
		REQ_PARAMS(1);
		sb_dsp_unsupported_command();
		break;
	
	case 0x10:	
		/* Direct 8-bit DAC - SB */
		REQ_PARAMS(1);
		S_printf("SB: Direct 8-bit DAC write (%u)\n", 
			SB_dsp.parameter[0]);
		sb_write_DAC(8, SB_dsp.parameter[0]);
	break;
	
	
		/* == OUTPUT COMMANDS == */

	case 0x14:
		/* DMA 8-bit DAC - SB */
		REQ_PARAMS(2);
		SB_dsp.length = PAR_LSB_MSB(0) + 1;
	        S_printf ("SB: 8-bit DMA output starting\n");
		SB_dsp.dma_mode &= ~DMA_AUTO_INIT;
		dma_start();
	break;

	case 0x16:
		/* DMA 2-bit ADPCM DAC - SB */
		REQ_PARAMS(2);
		SB_dsp.length = PAR_LSB_MSB(0) + 1;
	        S_printf("SB: Unsupported DMA type (0x%x)\n", SB_dsp.command);
		SB_dsp.dma_mode &= ~DMA_AUTO_INIT;
		/* dma_start(); */
	break;

	case 0x17:
		/* DMA 2-bit ADPCM DAC (Reference) - SB */
		REQ_PARAMS(2);
		SB_dsp.length = PAR_LSB_MSB(0) + 1;
	        S_printf("SB: Unsupported DMA type (0x%x)\n", SB_dsp.command);
		SB_dsp.dma_mode &= ~DMA_AUTO_INIT;
		/* dma_start(); */
	break;
	
	case 0x1C:	
		/* DMA 8-bit DAC (Auto-Init) - SB2.0 */
		if (SB_info.version < SB_20) {
			S_printf("SB: 8-bit Auto-Init DMA DAC not supported on this SB version.\n");
		}
		if (!SB_dsp.length) {
		/* 
		 * There seem to be two(!) ways to use the instruction
		 * 0x1C (8bit, lowspeed, auto): 
		 *   1. Set blocklength via 0x48, BLOCK, LENGTH
		 *      Issue 0x1C parameterless.
		 *   2. Issue 0x1C with blocklength as parameter, WITHOUT
		 *      setting it before. (This is wrong - 'sblaster.doc')
		 * I assume, no program ever uses BOTH ways at once, so
 		 * I remember, wether blocklength was set, and if, I expect
 		 * the first variation, if not, i expect the second. The flag
 		 * is reset when resetting the SB. The game "Millenia", which
 		 * is using the HMI-Driverkit uses the first way, but the
 		 * Shareware Version of Jazz Jackrabit uses the second one.
 		 * Does anyone know, which programmer to flame, and also know,
 		 * what his e-mail adress is? - Karcher
 		 */
		  REQ_PARAMS(2);
		  SB_dsp.length = PAR_LSB_MSB(0) + 1;
		  S_printf("SB: Warning: your program uses an undocumented feature of 0x1C command!\n");
		  S_printf("SB: Length is now set to %d\n", SB_dsp.length);
		}
	        S_printf ("SB: 8-bit DMA (Auto-Init) output starting\n");
		SB_dsp.dma_mode |= DMA_AUTO_INIT;
		dma_start();
	break;
	
	case 0x1F:	
		/* DMA 2-bit ADPCM DAC (Reference, Auto-Init) - SB2.0 */
		if (SB_info.version < SB_20) {
			S_printf("SB: 2-bit Auto-Init DMA DAC not supported on this SB version.\n");
		}
	        S_printf("SB: Unsupported DMA type (0x%x)\n", SB_dsp.command);
		SB_dsp.dma_mode |= DMA_AUTO_INIT;
		/* dma_start(); */
	break;


		/* == INPUT COMMANDS == */

	case 0x20:	
		/* Direct 8-bit ADC - SB */
		S_printf ("SB: 8-bit ADC (Unimplemented)\n");
 		dsp_write_output (0);
		break;
	break;

	case 0x24:
		/* DMA 8-bit ADC - SB */
		REQ_PARAMS(2);
		SB_dsp.length = PAR_LSB_MSB(0) + 1;
	        S_printf ("SB: 8-bit DMA input starting\n");
		SB_dsp.dma_mode &= ~DMA_AUTO_INIT;
		dma_start();
	break;

	case 0x28:
		/* Direct 8-bit ADC (Burst) - SBPro2 */
		if (SB_info.version > SB_PRO) {
			S_printf("SB: Burst mode 8-bit ADC (Unimplemented)\n");
		}
 		dsp_write_output (0);
		break;
			
	case 0x2C:	
		/* DMA 8-bit ADC (Auto-Init) - SB2.0 */
		if (SB_info.version < SB_20) {
			S_printf("SB: 8-bit Auto-Init DMA ADC not supported on this SB version.\n");
		}
		if (!SB_dsp.length) {
		  REQ_PARAMS(2);
		  SB_dsp.length = PAR_LSB_MSB(0) + 1;
		  S_printf("SB: Warning: your program uses an undocumented feature of 0x2C command!\n");
		  S_printf("SB: Length is now set to %d\n", SB_dsp.length);
		}
		SB_dsp.dma_mode |= DMA_AUTO_INIT;
	        S_printf ("SB: 8-bit DMA (Auto-Init) input starting\n");
		dma_start();
	break;


		/* == MIDI == */

	case 0x30:
		/* Midi Read Poll - SB */
		S_printf("SB: Attempt to read from SB Midi (Poll)\n");
		dsp_write_output(0);
		sb_do_midi_write();
		break;

	case 0x31:
		/* Midi Read Interrupt - SB */
		S_printf("SB: Attempt to read from SB Midi (Interrupt)\n");
		sb_do_midi_write();
		break;

	case 0x32:
		/* Midi Read Timestamp Poll - SB ? */
		S_printf("SB: Attempt to read from SB Midi (Poll)\n");
		dsp_write_output(0);
		dsp_write_output(0);
		dsp_write_output(0);
		dsp_write_output(0);
		sb_do_midi_write();
		break;

	case 0x33:
		/* Midi Read Timestamp Interrupt - SB ? */
		sb_do_midi_write();
		break;

	case 0x35:
		/* Midi Read/Write Poll (UART) - SB2.0 */
	case 0x36:
		/* Midi Read Interrupt/Write Poll (UART) - SB2.0 ? */
	case 0x37:
		/* Midi Read Interrupt Timestamp/Write Poll (UART) - SB2.0? */
		REQ_PARAMS(1);
		if (SB_info.version >= SB_20) {
			S_printf ("SB: Write 0x%u to SB Midi Port\n",
					  SB_dsp.parameter[0]);
				/* Command Ends _only_ on reset */
		}
		sb_do_midi_write();
		break;

	case 0x38:
		/* Midi Write Poll - SB */
		REQ_PARAMS(1);
		S_printf ("SB: Write 0x%u to SB Midi Port\n",
			  SB_dsp.parameter[0]);
		sb_do_midi_write();
		break;
	

		/* == SAMPLE SPEED == */

	case 0x40:
		/* Set Time Constant */
		REQ_PARAMS(1);
		SB_dsp.time_constant = SB_dsp.parameter[0];
		SB_dsp.sample_rate = 0;
		S_printf("SB: Time constant set to %u\n", SB_dsp.time_constant);
		break;

	case 0x41:
		/* Set Sample Rate - SB16 */
		REQ_PARAMS(2);
		if (SB_info.version < SB_16) {
		    S_printf("SB: MSB-LSB Sampling rate is not supported on this DSP version\n");
		}
		        /* MSB, LSB values - Karcher */
		SB_dsp.sample_rate = PAR_MSB_LSB(0);
		SB_dsp.time_constant = 0;
		S_printf("SB: Sampling rate set to %uHz\n", SB_dsp.sample_rate);
		break;


	/* == OUTPUT == */
	
	case 0x45:
		/* Continue Auto-Init 8-bit DMA - SB16 */
		restart_dsp_dma();
	break;
	
	case 0x47:
		/* Continue Auto-Init 16-bit DMA - SB16 */
		restart_dsp_dma();
	break;
	
	case 0x48:	
		/* Set DMA Block Size - SB2.0 */
		REQ_PARAMS(2);
		SB_dsp.length = PAR_LSB_MSB(0) + 1;
		S_printf("SB: Transfer length is set to %d\n", SB_dsp.length);
		break;

	case 0x74:
		/* DMA 4-bit ADPCM DAC - SB */
		REQ_PARAMS(2);
		SB_dsp.length = PAR_LSB_MSB(0) + 1;
	        S_printf("SB: Unsupported DMA type (0x%x)\n", SB_dsp.command);
		SB_dsp.dma_mode &= ~DMA_AUTO_INIT;
		/* dma_start(); */
	break;

	case 0x75:
		/* DMA 4-bit ADPCM DAC (Reference) - SB */
		REQ_PARAMS(2);
		SB_dsp.length = PAR_LSB_MSB(0) + 1;
	        S_printf("SB: Unsupported DMA type (0x%x)\n", SB_dsp.command);
		SB_dsp.dma_mode &= ~DMA_AUTO_INIT;
		/* dma_start(); */
	break;

	case 0x76:
		/* DMA 2.6-bit ADPCM DAC - SB */
		REQ_PARAMS(2);
		SB_dsp.length = PAR_LSB_MSB(0) + 1;
	        S_printf("SB: Unsupported DMA type (0x%x)\n", SB_dsp.command);
		SB_dsp.dma_mode &= ~DMA_AUTO_INIT;
		/* dma_start(); */
	break;

	case 0x77:
		/* DMA 2.6-bit ADPCM DAC (Reference) - SB */
		REQ_PARAMS(2);
		SB_dsp.length = PAR_LSB_MSB(0) + 1;
	        S_printf("SB: Unsupported DMA type (0x%x)\n", SB_dsp.command);
		SB_dsp.dma_mode &= ~DMA_AUTO_INIT;
		/* dma_start(); */
	break;

	case 0x7D:
		/* DMA 4-bit ADPCM DAC (Auto-Init, Reference) - SB2.0 */
		if (SB_info.version < SB_20) {
			S_printf ("SB: 4-bit Auto-Init ADPCM DMA DAC not supported on this SB version.\n");
		}
	        S_printf("SB: Unsupported DMA type (0x%x)\n", SB_dsp.command);
		SB_dsp.dma_mode |= DMA_AUTO_INIT;
		/* dma_start(); */
	break;

	case 0x7F:
		/* DMA 2.6-bit ADPCM DAC (Auto-Init, Reference) - SB2.0 */
		if (SB_info.version < SB_20) {
			S_printf ("SB: 2.6-bit Auto-Init ADPCM DMA DAC not supported on this SB version.\n");
		}
	        S_printf("SB: Unsupported DMA type (0x%x)\n", SB_dsp.command);
		SB_dsp.dma_mode |= DMA_AUTO_INIT;
		/* dma_start(); */
	break;
	
	case 0x80:
		/* Silence DAC - SB */
		REQ_PARAMS(2);
		SB_dsp.length = PAR_LSB_MSB(0) + 1; 
		sb_write_silence();
		break;

	case 0x90:
		/* DMA 8-bit DAC (High Speed, Auto-Init) - SB2.0-Pro2 */
    		if (SB_info.version < SB_20 /*|| SB_info.version > SB_PRO*/)  {
    			S_printf ("SB: 8-bit Auto-Init High Speed DMA DAC not supported on this SB version.\n");
    		}
		if (!SB_dsp.length) {
		  REQ_PARAMS(2);
		  SB_dsp.length = PAR_LSB_MSB(0) + 1;
		  S_printf("SB: Length is now set to %d\n", SB_dsp.length);
		}
		S_printf ("SB: 8-bit DMA (High Speed, Auto-Init) output starting\n");
		SB_dsp.dma_mode |= HIGH_SPEED_DMA | DMA_AUTO_INIT;
		dma_start();
	break;


        case 0x91:
	        /* **CRISK** DMA-8 bit DAC (High Speed) */
    		if (SB_info.version < SB_20 /*|| SB_info.version > SB_PRO*/)  {
    			S_printf ("SB: 8-bit High Speed DMA DAC not supported on this SB version.\n");
    		}
		if (!SB_dsp.length) {
		  REQ_PARAMS(2);
		  SB_dsp.length = PAR_LSB_MSB(0) + 1;
		  S_printf("SB: Length is now set to %d\n", SB_dsp.length);
		}
	        S_printf ("SB: 8-bit DMA (High Speed) output starting\n");
		SB_dsp.dma_mode &= ~DMA_AUTO_INIT;
		SB_dsp.dma_mode |= HIGH_SPEED_DMA;
	        dma_start();
	break;
	 
	/* == INPUT == */
	
	case 0x98:
		/* DMA 8-bit ADC (High Speed, Auto-Init) - SB2.0-Pro2 */
		if (SB_info.version < SB_20 || SB_info.version > SB_PRO) {
			S_printf ("SB: 8-bit Auto-Init High Speed DMA ADC not supported on this SB version.\n");
		}
		if (!SB_dsp.length) {
		  REQ_PARAMS(2);
		  SB_dsp.length = PAR_LSB_MSB(0) + 1;
		  S_printf("SB: Length is now set to %d\n", SB_dsp.length);
		}
	        S_printf ("SB: 8-bit DMA (High Speed, Auto-Init) input starting\n");
		SB_dsp.dma_mode |= HIGH_SPEED_DMA | DMA_AUTO_INIT;
		dma_start();
	break;

	case 0x99:
		/* DMA 8-bit ADC (High Speed) - SB2.0-Pro2 */
		if (SB_info.version < SB_20 || SB_info.version > SB_PRO) {
			S_printf ("SB: 8-bit High-Speed DMA ADC not supported on this SB version.\n");
		}
		if (!SB_dsp.length) {
		  REQ_PARAMS(2);
		  SB_dsp.length = PAR_LSB_MSB(0) + 1;
		  S_printf("SB: Length is now set to %d\n", SB_dsp.length);
		}
	        S_printf ("SB: 8-bit DMA (High Speed) input starting\n");
		SB_dsp.dma_mode &= ~DMA_AUTO_INIT;
		SB_dsp.dma_mode |= HIGH_SPEED_DMA;
		dma_start();
	break;
	
	
		/* == STEREO MODE == */

	case 0xA0:	
		/* Enable Mono Input - SBPro Only */
		S_printf ("SB: Disable Stereo Input not implemented\n");
	break;

	case 0xA8:
		/* Enable Stereo Input - SBPro Only */
		S_printf ("SB: Enable Stereo Input not implemented\n");
	break;
	

	/* == SB16 direct ADC/DAC == */

	case 0xB0 ... 0xBF:
 		/* SB16 16-bit DMA */
		REQ_PARAMS(3);
		S_printf("SB: 16-bit DMA is not supported yet\n");
	break;

	case 0xC0 ... 0xCF:
		if (SB_dsp.command & 1) {
		  sb_dsp_unsupported_command();
		  break;
		}
 		/* SB16 8-bit DMA */
		REQ_PARAMS(3);
		if(SB_info.version < SB_16) {
		  S_printf("SB: SB16-DMA not supported on this DSP-version\n");
		}
		SB_dsp.length = PAR_LSB_MSB(1) + 1;
		if (SB_dsp.command & 4)
		  SB_dsp.dma_mode |= DMA_AUTO_INIT;
		else
		  SB_dsp.dma_mode &= ~DMA_AUTO_INIT;
		if (SB_dsp.command & 8) {
		  S_printf("SB: Starting SB16 8-bit DMA input\n");
		}
		else {
		  S_printf("SB: Starting SB16 8-bit DMA output\n");
		}
		if (SB_dsp.parameter[0] & 16) {
		  S_printf("SB: Warning: signed sampling is not supported yet\n");
		}
		SB_dsp.stereo = (SB_dsp.parameter[0] & 32) ? 1 : 0;
		S_printf("SB: stereo set to %i\n", SB_dsp.stereo);
		dma_start();
	break;


	/* == DMA == */
	
	case 0xD0:
		/* Halt 8-bit DMA - SB */
	pause_dsp_dma(); 
	break;
	

		/* == SPEAKER == */
		
	case 0xD1:
		/* Enable Speaker - SB */
		sb_enable_speaker();
	break;
	
	case 0xD3:
		/* Disable Speaker - SB */
		sb_disable_speaker();
	break;
	
	
		/* == DMA == */

	case 0xD4:
		/* Continue 8-bit DMA - SB */
		restart_dsp_dma();
	break;

	case 0xD5:
		/* Halt 16-bit DMA - SB16 */
		pause_dsp_dma();
		break;

	case 0xD6:
		/* Continue 16-bit DMA - SB16 */
		restart_dsp_dma();
	break;
	

		/* == SPEAKER == */
		
	case 0xD8:
		/* Speaker Status */
		dsp_write_output (SB_info.speaker);
		break;
	
	/* == DMA == */
	
	case 0xD9:
		/* Exit Auto-Init 16-bit DMA - SB16 */
	case 0xDA:
		/* Exit Auto-Init 8-bit DMA - SB2.0 */
		SB_dsp.dma_mode &= ~DMA_AUTO_INIT;
		break;

	
	/* == DSP IDENTIFICATION == */
	
	case 0xE0:
		/* DSP Identification - SB2.0 */
		REQ_PARAMS(1);
		if (SB_info.version >= SB_20) {
			S_printf("SB: Identify SB2.0 DSP.\n");
			dsp_write_output(~SB_dsp.parameter[0]);
		}
		else
			S_printf("SB: Identify DSP not supported by this SB version.\n");
	break;

	case 0xE1:
		/* DSP Version - SB */
		sb_get_version();
		break;

	case 0xE2: {
		/* DMA Identification - completely undocumented! */
		/* Code stolen from VDMSound project,
		 * http://www.sourceforge.net/projects/vdmsound/
		 */
		int i;
		REQ_PARAMS(1);
		SB_dsp.length = 1;
		for (i = 0; i < 8; i++)
		  if ((SB_dsp.parameter[0] >> i) & 0x01)
		    m_E2Value += E2_incr_table[m_E2Count % 4][i];

		m_E2Value += E2_incr_table[m_E2Count % 4][8];
		m_E2Count++;
		E2_Active = 1;
		dma_start();
		break;
	}

	case 0xE3:
		/* DSP Copyright - SBPro2 ? */
	  dsp_do_copyright();
		break;

	case 0xE4:
		/* Write to Test - SB2.0 */
		REQ_PARAMS(1);
		if (SB_info.version >= SB_20) {
			SB_dsp.test = SB_dsp.parameter[0];
			S_printf("SB: write 0x%x to test register.\n", SB_dsp.test);
		}
		else {
			S_printf("SB: write to test register not in this SB version.\n");
		}
	break;


		/* == TEST == */

	case 0xE8:
		/* Read from Test - SB2.0 */
		if (SB_info.version >= SB_20) {
			S_printf("SB: Read 0x%x from test register.\n", 
				 SB_dsp.test);
			dsp_write_output(SB_dsp.test);
			}
			else {
			S_printf("SB: read from test register not supported by SB version.\n");
		}
		break;

	case 0xF0:
		/* Sine Generator - SB */
		sb_do_sine();
		break;


		/* == STATUS == */

	case 0xF1:	
		/* DSP Auxiliary Status - SBPro2 */
		sb_get_aux_status();
		break;


		/* == IRQ == */

	case 0xF2:	
		/* 8-bit IRQ - SB */
	        sb_activate_irq (SB_IRQ_8BIT);
		break;

	case 0xF3:
		/* 16-bit IRQ - SB16 */
	        sb_activate_irq (SB_IRQ_16BIT);
		break;

		/* 0xFB: DSP Status - SB16 */
		/* 0xFC: DSP Auxiliary Status - SB16 */
		/* 0xFD: DSP Command Status - SB16 */

	default:
		sb_dsp_unsupported_command();
      }

      SB_dsp.command = SB_NO_DSP_COMMAND;
      SB_dsp.num_parameters = 0;

/* 
 * Some programs expects DSP to be busy after a command was written to it, but
 * not immediately. Probably it takes some time for the command being written
 * to the i/o port, to be accepted by DSP.
 * So I am going to return 0 (ready), then 0x80 (busy) and then 0 again.
 *
 * -- Stas Sergeev
 */
        DSP_busy_hack = 2;
}


/* 
 * DANG_FIXTHIS DSP Status is unimplemented 
 */
void sb_dsp_get_status (void) 
{
	Bit8u output = 0;

	S_printf("SB: Request for SB2.0/SBPRO status (Unimplemented)\n");

	if (SB_info.version != SB_20 && SB_info.speaker) {
		output |= 1;
	}

	dsp_write_output(output);
}
	
void sb_dsp_unsupported_command (void)
{	
    S_printf ("SB: Unsupported Command 0x%x (parameter 0x%x)\n",
	SB_dsp.command, SB_dsp.parameter[0]);
}


void sb_write_silence (void)
{
void *silence_data;

		/*
		 * DANG_BEGIN_REMARK
		 * Write silence could probably be implemented by setting up a
		 * "DMA" transfer from /dev/null - AM
		 * DANG_END_REMARK
		 */

     /* DANG_FIXME sb_write_silence should take into account the sample type */

		/* 
		 * Originally from Karcher using a special function, 
		 * generalized by Alistair
		 */

		 
		if(SB_dsp.stereo)
		  SB_dsp.length *= 2;
		
		silence_data = malloc(SB_dsp.length);
		if (silence_data == NULL) {
		  S_printf("SB: Failed to alloc memory for silence. Aborting\n");
		  sb_activate_irq(SB_IRQ_8BIT);
		  return;
		}

		/* Assuming _signed_ samples silence is 0x80 */
		memset (silence_data, 0x80, SB_dsp.length);

		if (SB_driver.play_buffer != NULL) {
		  (*SB_driver.play_buffer)(silence_data, SB_dsp.length);
                  SB_dsp.empty_state = IRQ_AT_EMPTY;
		}
		else {
		  if (debug_level('S') >= 3)
		    S_printf ("SB: Optional function 'play_buffer' not provided.\n");
		  sb_activate_irq(SB_IRQ_8BIT);
		}
}

void adlib_io_write(ioport_t port, Bit8u value)
{
    /* Base Port for Adlib is 0x388 */
    /* Base Port for Adv. Adlib is 0x38a */

    switch (port) {
	case 0x388:		/* Adlib Register Port */
		S_printf("Adlib: Write 0x%x to Register Port\n", value);
		fm_io_write(ADLIB_REGISTER, value);
		break;
	case 0x389:		/* Adlib Data Port */
		S_printf("Adlib: Write 0x%x to Data Port\n", value);
		fm_io_write(ADLIB_DATA, value);
	break;
	
	case 0x38A:		/* Adv. Adlib Register Port */
		S_printf("Adv_Adlib: Write 0x%x to Register Port\n", value);
		fm_io_write(ADV_ADLIB_REGISTER, value);
		break;
	case 0x38B:		/* Adv. Adlib Data Port */
		S_printf("Adv_Adlib: Write 0x%x to Data Port\n", value);
		fm_io_write(ADV_ADLIB_DATA, value);
		break;
    };
}
	
void fm_io_write(ioport_t port, Bit8u value)
{
  extern struct adlib_info_t adlib_info;
  extern struct adlib_timer_t adlib_timers[2];

    switch (port) {
	case ADLIB_REGISTER:
	  adlib_info.reg = value;

	break;

	case ADLIB_DATA:
	  switch (adlib_info.reg) {
	  case 0x01: /* Test LSI/Enable Waveform control */
	    /* DANG_FIXTHIS Adlib Waveform tests are unimplemented */
	    break;
	  case 0x02: /* Timer 1 data */
	    S_printf ("Adlib: Timer 1 register set to %d\n", value);
	    adlib_timers[0].reg = value;
	    break;
	  case 0x03: /* Timer 2 data */
	    S_printf ("Adlib: Timer 2 register set to %d\n", value);
	    adlib_timers[1].reg = value;
	    break;
	  case 0x04: /* Timer control flags */
	    if (value & 0x80) {
	      S_printf ("Adlib: Resetting both timers\n");

	      adlib_timers[0].enabled = 0;
	      adlib_timers[1].enabled = 0;
	      sb_is_running &= (~FM_TIMER_RUN);

	      adlib_timers[0].expired = 0;
	      adlib_timers[1].expired = 0;
	      return;
	    }
 	    if ( !(value & 0x40) ) {
	      if (value & 1) {
		S_printf ("Adlib: Timer 1 counter set to %d\n", adlib_timers[0].reg);
		adlib_timers[0].counter = adlib_timers[0].reg;
		adlib_timers[0].enabled = 1;
		adlib_timers[0].expired = 0;
		sb_is_running |= FM_TIMER_RUN;
	      } else {
		S_printf ("Adlib: Timer 1 disabled\n");
		adlib_timers[0].enabled = 0;
	      }
	    }
 	    if ( !(value & 0x20) ) {
	      if (value & 2) {
		S_printf ("Adlib: Timer 2 counter set to %d\n", adlib_timers[1].reg);
		adlib_timers[1].counter = adlib_timers[1].reg;
		adlib_timers[1].enabled = 1;
		adlib_timers[1].expired = 0;
		sb_is_running |= FM_TIMER_RUN;
	      } else {
		S_printf ("Adlib: Timer 2 disabled\n");
		adlib_timers[1].enabled = 0;
	      }
	    }

	    sb_update_timers();

	    break;
	  default: /* unhandled */
	    S_printf ("Adlib: Data Writes are unimplemented\n");
	    break;
	  }
		break;
	
	case ADV_ADLIB_REGISTER:
		/* DANG_FIXTHIS Advanced Adlib register writes are unimplemented */
		break;

	case ADV_ADLIB_DATA:
		/* DANG_FIXTHIS Advanced Adlib data writes are unimplemented */
		break;
	
    };
}


void mpu401_io_write(ioport_t port, Bit8u value)
{
	__u32 addr;

	addr = port - config.mpu401_base;

	switch (addr) {
	case 0:
		/* Write data port */
		S_printf("MPU401: Write 0x%02x to data port\n",value);
		(*mpu401_info.data_write)(value);
		break;
	case 1:
		/* Write command port */
		S_printf("MPU401: Write 0x%02x to command port\n",value);
		switch (value) {
		  case 0x3f:		// 0x3F = UART mode
		    sb_activate_irq(SB_IRQ_MIDI);
		    break;
		  case 0xff:		// 0xFF = reset MPU. Does anyone know more?
		    break;
		}
		mpu401_info.isdata=1; /* A command is sent: MPU_ACK it next time */
		break;
	}
}


/* 
 * DANG_FIXTHIS SB Midi is Unimplemented 
 */

void sb_do_midi_write (void) 
{
	S_printf("SB: Sorry, unimplemented MIDI command 0x%x\n", SB_dsp.command);
}


static void dsp_do_copyright(void)
{
#ifndef STRICT_SB_EMU
	char cs[] = "(c) Copyright 1995 Alistair MacDonald";
#else /* STRICT_SB_EMU */
	char cs[] = "Copyright (c) Creative Technology";
#endif
	char *ptr;

	if (SB_info.version > SB_PRO) {
		S_printf("SB: DSP Copyright requested.\n");
		ptr = cs;
		do {
			dsp_write_output((__u8) * ptr);
		} while (*ptr ++);
	} else {
		S_printf("SB: DSP copyright not supported by this SB Version.\n");
	}

	SB_dsp.command = SB_NO_DSP_COMMAND;
}


/* 
 * DANG_FIXTHIS Sine Generation is unimplemented 
 */
void sb_do_sine (void)
{
	S_printf("SB: Start Sine generator. (unimplemented)\n");
	
	if (SB_info.version < SB_16) {
		sb_enable_speaker();
	}
}

/* 
 * DANG_FIXTHIS AUX Status is Unimplemented 
 */
void sb_get_aux_status (void)
{
	if (SB_info.version > SB_PRO) {
		S_printf("SB: DSP aux. status (unimplemented)\n");
		dsp_write_output(18);
	}
	else {
		S_printf("SB: DSP aux. status not supported by SB version.\n");
	}
}

void sb_get_version (void)
{
	S_printf("SB: Query Version\n");
	dsp_write_output(SB_info.version >> 8);
	dsp_write_output(SB_info.version & 0xFF);
}

/*
 * DMA Support
 * ===========
 */
 
#define SB_IRQ_PEND (SB_info.irq.pending & (SB_IRQ_8BIT | SB_IRQ_16BIT))
void pause_dsp_dma(void)
{
  S_printf("SB: Pausing DMA transfer, %d bytes left\n", SB_dsp.bytes_left);
  if (SB_driver.DMA_pause != NULL) {
    (*SB_driver.DMA_pause)();
  }
  else if (debug_level('S') >= 3) {
    S_printf ("SB: Optional function 'DMA_pause' not provided.\n");
  }

  if(!SB_dsp.pause_state)
    dma_drop_DREQ(config.sb_dma);
  sb_deactivate_irq(SB_IRQ_PEND);
  dma_pending = 0;
  sb_is_running &= ~DSP_OUTPUT_RUN;
  SB_dsp.pause_state = 1;
}

void restart_dsp_dma(void)
{
  if (!SB_dsp.pause_state)
    return;
  if (SB_driver.DMA_resume != NULL) {
    (*SB_driver.DMA_resume)();
  }
  else if (debug_level('S') >= 3) {
    S_printf ("SB: Optional function 'DMA_resume' not provided.\n");
  }
   
  SB_dsp.pause_state = 0;
  sb_is_running |= DSP_OUTPUT_RUN;
  if (!SB_dsp.empty_state)
    dma_assert_DREQ(config.sb_dma);
}

void sb_dma_done_block(void)
{
  S_printf("SB: DMA block completed\n");
  if (dma_test_DACK(config.sb_dma))
    S_printf("SB: DMA transfer continues (Auto-Init)\n");
}

void sb_dma_start(void)
{
  if (dma_pending)
    start_dsp_dma();
}

void start_dsp_dma(void)
{
  static int dma_transfer_length;
  int result = 0;
  int real_sampling_rate;

  S_printf("SB: Starting to open DMA access to DSP (%s mode)\n",
    (SB_dsp.dma_mode & DMA_AUTO_INIT) ? "Auto-Init" : "Single-Cycle");

  SB_dsp.empty_state &= ~START_DMA_AT_EMPTY;
  sb_is_running |= DSP_OUTPUT_RUN;

  if (!SB_dsp.length) {
    S_printf("SB: ERROR: transfer lenght is not set, aborting DMA!\n");
    sb_is_running &= ~DSP_OUTPUT_RUN;
    return;
  }

  if (SB_dsp.bytes_left && !SB_dsp.pause_state &&
   !(SB_dsp.empty_state & DMA_CONTINUE) && (SB_dsp.dma_mode & SB_USES_DMA)) {
      S_printf("SB: Waiting DMA transfer cycle to finish, %d bytes left\n",
        SB_dsp.bytes_left);
      SB_dsp.empty_state |= START_DMA_AT_EMPTY;
      return;
    }

 /* drop DRQ for now - DMA startup can fail */
  dma_drop_DREQ(config.sb_dma);

 if (SB_info.speaker) {
  if (SB_dsp.sample_rate && SB_dsp.time_constant)
    S_printf("SB: ERROR: Both sampling rate and time constant are set!\n");
  if (SB_dsp.sample_rate)
    real_sampling_rate = SB_dsp.sample_rate;
  else {
    real_sampling_rate = 1000000 / (256 - SB_dsp.time_constant);
    if (SB_dsp.stereo)
      real_sampling_rate /= 2;
    if (!SB_dsp.time_constant)
      S_printf("SB: ERROR: Sampling rate is not set!\n");
  }
  S_printf("SB: Sampling rate is %uHz\n", real_sampling_rate);

  /* Set up the housekeeping for the DMA transfer */
  dma_transfer_length = MIN(SB_dsp.length/2, MAX_DMA_TRANSFERSIZE);
  if(dma_transfer_length < MAX_DMA_TRANSFERSIZE &&
   dma_transfer_length * 2 < SB_dsp.length)
    dma_transfer_length++;
  dma_transfer_length = MIN(dma_transfer_length,
    dma_get_block_size(config.sb_dma));
  if (dsp_block_size != SB_dsp.length) {
    if (SB_driver.DMA_set_blocksize != NULL)
      (*SB_driver.DMA_set_blocksize)
	(MAX(SB_dsp.length/2, MAX_DMA_TRANSFERSIZE*4), dma_transfer_length);
    dsp_block_size = SB_dsp.length;
  }

  if (SB_driver.set_speed == NULL) {
    S_printf ("SB: Required function 'DMA_set_speed' not provided, can't use DMA.\n");
    sb_is_running &= ~DSP_OUTPUT_RUN;
    return;
  }
  if (SB_driver.DMA_start_init == NULL) {
    S_printf ("SB: Required function 'DMA_start_init' not provided.\n");
  }
  else
    result = (*SB_driver.DMA_start_init)();
  if (result == 0)
    result = (*SB_driver.set_speed) (real_sampling_rate, SB_dsp.stereo);
  if (result<0) {
    SB_dsp.empty_state |= START_DMA_AT_EMPTY;
    dsp_block_size = 0;
    S_printf ("SB: DMA init failed, will try again later...\n");
    return;
  }
 } else {
    S_printf ("SB: Speaker not enabled\n");
    dma_transfer_length = MIN(SB_dsp.length, MAX_DMA_TRANSFERSIZE);
    dma_transfer_length = MIN(dma_transfer_length,
      dma_get_block_size(config.sb_dma));
    /* we want only one irq when speaker not enabled */
    SB_dsp.dma_mode &= ~DMA_AUTO_INIT;
 }

  dma_set_transfer_size(config.sb_dma, dma_transfer_length);
  S_printf("SB: DSP block size is set to %d\n", SB_dsp.length);
  S_printf("SB: DMA block size is %ld (%ld left)\n",
    dma_get_block_size(config.sb_dma), dma_bytes_left(config.sb_dma));
  S_printf("SB: DMA transfer size is set to %d\n", dma_transfer_length);

  SB_dsp.pause_state = 0;
  if (!(SB_dsp.empty_state & DMA_CONTINUE))
    SB_dsp.bytes_left = SB_dsp.length;
  else
    S_printf("SB: Resuming DMA transfer, %d bytes left\n", SB_dsp.bytes_left);
  SB_dsp.empty_state &= ~DMA_CONTINUE;
  SB_dsp.dma_mode |= SB_USES_DMA;
  dma_pending = 0;
//  byte_skipped = 0;		/* uncomment this for some reverberation :) */

  /* Commence Firing ... */
  dma_assert_DREQ(config.sb_dma);
}

static void handle_dma_IO(int size)
{
  sb_is_running |= DSP_OUTPUT_RUN;
  dma_drop_DREQ(config.sb_dma);

  if (size > SB_dsp.bytes_left) {
    error("SB: DMA crossed transfer buffer! (size=%d left=%d)\n",
	size, SB_dsp.bytes_left);
    size = SB_dsp.bytes_left;
  }

  SB_dsp.bytes_left -= size;
     
  if(SB_dsp.bytes_left)
  {
    if(!(SB_dsp.dma_mode & SB_USES_DMA)) {	/* what? Damn this ST3... */
      S_printf("SB: Warning: DMA transfer interrupted!\n");
      return;
    }
    /* Still some bytes left till IRQ */
    if(!SB_dsp.pause_state)
      dma_assert_DREQ(config.sb_dma);
    else
      sb_is_running &= ~DSP_OUTPUT_RUN;
  }
  else {
    /* We are at the end of an block */
    S_printf("SB: Done block, triggering IRQ\n");
    if(SB_dsp.dma_mode & DMA_AUTO_INIT)
    {
      /* Reset block size */
      SB_dsp.bytes_left = SB_dsp.length;
      sb_activate_irq(SB_IRQ_8BIT);
      if (!SB_dsp.empty_state) {
	S_printf("SB: Auto-reinitialized for next block\n");

	/* bad HACK :( */
	if (test_bit(SB_info.irq.irq8, &pic0_imr)) {
          S_printf("SB: Warning: SB IRQ (%d) is masked!!!\n", config.sb_irq);
	  dma_assert_DREQ(config.sb_dma);
	  SB_dsp.empty_state &= ~DREQ_AT_EOI;
	}
        else
	  SB_dsp.empty_state |= DREQ_AT_EOI;
      }
      else
        S_printf("SB: Warning: empty_state = %d, aborting Auto-Init DMA!\n",
	  SB_dsp.empty_state);
    }
    else {
      SB_dsp.dma_mode &= ~SB_USES_DMA;
      if((SB_dsp.length > dma_get_block_size(config.sb_dma)) ||
	(SB_dsp.empty_state & START_DMA_AT_EMPTY)) {
        sb_activate_irq(SB_IRQ_8BIT);
	if(SB_dsp.empty_state & START_DMA_AT_EMPTY) {
/* we are probably switching from Auto-init to Single, so try to start DMA ASAP */
	  start_dsp_dma();
	}
      }
      else {
	SB_dsp.empty_state |= IRQ_AT_EMPTY;
	if (!pic_icount)		/* broken PIC, ST3 HACK */
	  sb_check_complete();
      }
    }
  }
}

size_t sb_dma_write(void *dosptr, size_t size)
{
int amount_done = 0, length;
void *ptr = dosptr;
Bit8u buffer[MAX_DMA_TRANSFERSIZE+1];
static Bit8u missed_byte;

  if (debug_level('S') >= 2) {
    S_printf ("SB: sb_dma_write: size=%d\n", size);
  }

  length = MIN(size, SB_dsp.bytes_left);
  if (!length)
    return 0;
  S_printf("SB: Going to write %d bytes\n", length);

  if (!SB_driver.DMA_do_write) {
    S_printf("SB: function \"DMA_do_write\" not provided, can't use DMA!\n");
    amount_done = length;
  }
  else {
    if (SB_info.speaker) {
      if (SB_dsp.stereo && ((length % 2) ^ byte_skipped)) {
        if (!byte_skipped)
          S_printf("SB: Warning: requested %d (odd) bytes in Stereo\n",length);
        length--;
        if (length == 0) {
/* 
 * The idea of this hack is to preserve the last byte and attach it to the
 * beginning of the next block. Outputting it now will result in unexpected
 * reverberation, dropping it will result in a channel swapping.
 * This HACK is ugly, esp. for the intermediate buffer involvement,
 * but suggest another solution or send me a patch - I'll be happy.
 *
 * -- Stas Sergeev
 */
          missed_byte = *(Bit8u *)dosptr;
          S_printf("SB: Warning: skipping byte 0x%hhx\n", missed_byte);
          handle_dma_IO(1);
          byte_skipped = 1;
          return 1;
        }
      }
      if (byte_skipped) {		/* UGLY, ugly, ugly!!! */
	S_printf("SB: Inserting missing byte 0x%hhx\n", missed_byte);
	buffer[0] = missed_byte;
        memcpy(buffer + 1, dosptr, length);
	ptr = buffer;
      }

      if (SB_dsp.stereo && ((length + byte_skipped) % 2))
        error("SB: ERROR: odd transfer in stereo mode!!!\n");

      amount_done = SB_driver.DMA_do_write(ptr, length + byte_skipped);

      if (SB_dsp.stereo && (amount_done % 2))
        error("SB: ERROR: Driver does odd transfer in stereo mode!!!\n");

      if (amount_done > 0) {
        amount_done -= byte_skipped;
	byte_skipped = 0;
      }
    } else {
      S_printf("SB: Speaker not enabled, doing nothing...\n");
      amount_done = length;
    }
  }

  handle_dma_IO(amount_done);

  if (debug_level('S') >= 2) {
    S_printf ("SB: Outputted %d bytes, %d bytes left\n",
	amount_done, SB_dsp.bytes_left);
  }

  return amount_done;
}

size_t sb_dma_read(void *ptr, size_t size)
{
int amount_done, length;
char fill;

  if (debug_level('S') >= 2) {
    S_printf ("SB: sb_dma_read: size=%d\n", size);
  }

  length = MIN(size, SB_dsp.bytes_left);
  if (!length)
    return 0;
  S_printf("SB: Going to read %d bytes\n", length);    

  if (!SB_driver.DMA_do_read) {
    S_printf("SB: function \"DMA_do_read\" not provided, can't use DMA!\n");
    amount_done = length;
  }
  else {
    if (SB_info.speaker)
      amount_done = SB_driver.DMA_do_read(ptr, length);
    else {
      fill = E2_Active ? m_E2Value : 0x80;
      S_printf("SB: Speaker not enabled, memset'ing buffer with 0x%02hhX\n", fill);
      memset(ptr, fill, length);
      E2_Active = 0;
      amount_done = length;
    }
  }

  handle_dma_IO(amount_done);

  if (debug_level('S') >= 2) {
    S_printf ("SB: Inputted %d bytes, %d bytes left\n",
	amount_done, SB_dsp.bytes_left);
  }

  return amount_done ;
}

void sb_irq_trigger (void)
{
  S_printf ("SB: Interrupt activated.\n");

  SB_info.irq.active |= SB_info.irq.pending;
  SB_info.irq.pending = 0;

  if (SB_driver.DMA_complete != NULL) {
    (*SB_driver.DMA_complete)();
  }
  else if (debug_level('S') >= 3) {
    S_printf ("SB: Optional function 'DMA_complete' not provided.\n");
  }

  into_irq = 1;
  do_irq();
  into_irq = 0;
}

static void sb_check_complete (void)
{
  int result = DMA_HANDLER_OK;
  int dma_change_speed = 1;

  if (SB_driver.DMA_complete_test != NULL) {
    result = (*SB_driver.DMA_complete_test)();
  }
  else if (debug_level('S') >= 3) {
    S_printf ("SB: Optional function 'DMA_complete_test' not provided.\n");
  }

  if(result == DMA_HANDLER_OK)
  {
    sb_is_running &= ~DSP_OUTPUT_RUN;
    if (SB_driver.DMA_can_change_speed == NULL)
      S_printf ("SB: Optional function 'DMA_can_change_speed' not provided.\n");
    else
      dma_change_speed = (*SB_driver.DMA_can_change_speed)();
    if(dma_change_speed) {
      if(SB_dsp.empty_state & START_DMA_AT_EMPTY) {
	S_printf("SB: Trying to start DMA transfer again...\n");
	start_dsp_dma();
      }
    }
    if(SB_dsp.empty_state & IRQ_AT_EMPTY)
      sb_activate_irq(SB_IRQ_8BIT);
    SB_dsp.empty_state &= ~IRQ_AT_EMPTY;
    if(!SB_dsp.bytes_left) {
      SB_dsp.dma_mode = 0;
    }

    if(SB_dsp.empty_state & DREQ_AT_EOI) {
      if (!into_irq && !SB_info.irq.pending) {
	S_printf("SB: Warning: program doesn't ACK the interrupt, enjoy clicking.\n");  
	if(!SB_dsp.pause_state)
	  dma_assert_DREQ(config.sb_dma);
	SB_dsp.empty_state &= ~DREQ_AT_EOI;
      }
      else
	S_printf("SB: Warning: DMA is running too slow!\n");
    }
  }

  if (SB_dsp.empty_state)
    sb_is_running |= DSP_OUTPUT_RUN;
}

/*
 * Sound Initialisation
 * ====================
 */

void sound_init(void)
{
  if (config.sound) {
    sb_init();
    fm_init();
    mpu401_init();
  }
}

static void sb_detect(void)
{
  /* First - Check if the DMA/IRQ values make sense */

  /* Must have an IRQ between 1 & 15 inclusive */
  if (config.sb_irq < 1 || config.sb_irq > 15 ) {
    S_printf ("SB: Invalid IRQ (%d). SB Disabled.\n", config.sb_irq);
    SB_info.version = SB_NONE;
    return;
  }

  /* Must have a DMA between 0 & 7, excluding 4 [unsigned, so don't test < 0]*/
  if (config.sb_dma == 4 || config.sb_dma > 7) {
    S_printf ("SB: Invalid DMA channel (%d). SB Disabled.\n", config.sb_dma);
    SB_info.version = SB_NONE;
    return;
  }

  SB_info.version = SB_driver_init();

  /* Karcher */
  if(SB_info.version > SB_PRO)
  {
    SB_info.version = SB_PRO;
    S_printf("SB: Downgraded emulation to SB Pro because SBEmu is incomplete\n");
  }

#ifdef STRICT_SB_EMU
  SB_info.version = STRICT_SB_EMU;
#endif
}

static void sb_init(void)
{
  emu_iodev_t  io_device;

  S_printf ("SB: SB Initialisation\n");

  /* SB Emulation */
  io_device.read_portb   = sb_io_read;
  io_device.write_portb  = sb_io_write;
  io_device.read_portw   = NULL;
  io_device.write_portw  = NULL;
  io_device.read_portd   = NULL;
  io_device.write_portd  = NULL;
  io_device.handler_name = "SB Emulation";
  io_device.start_addr   = config.sb_base;
  io_device.end_addr     = config.sb_base+ 0x013;
  io_device.irq          = config.sb_irq;
  io_device.fd           = -1;
  if (port_register_handler(io_device, 0) != 0) {
    S_printf ("SB: Error registering DSP port handler\n");
    SB_info.version = SB_NONE;
  }

  /* Register the Interrupt */

  SB_info.irq.irq8 = pic_irq_list[config.sb_irq];

  /* We let DOSEMU handle the interrupt */
  pic_seti(SB_info.irq.irq8, sb_irq_trigger, 0, NULL);
  pic_unmaski(SB_info.irq.irq8);

  /* Register DMA handlers */
  dma_install_handler(config.sb_dma, sb_dma_read, sb_dma_write, sb_dma_start,
    sb_dma_done_block);

  S_printf ("SB: Initialisation - Base 0x%03x, IRQ %d, DMA %d\n", 
	    config.sb_base, config.sb_irq, config.sb_dma);
}

static void fm_init(void)
{
  emu_iodev_t  io_device;
  
  S_printf ("SB: FM Initialisation\n");

  /* This is the FM (Adlib + Advanced Adlib) */
  io_device.read_portb   = adlib_io_read;
  io_device.write_portb  = adlib_io_write;
  io_device.read_portw   = NULL;
  io_device.write_portw  = NULL;
  io_device.read_portd   = NULL;
  io_device.write_portd  = NULL;
  io_device.handler_name = "Adlib (+ Advanced) Emulation";
  io_device.start_addr   = 0x388;
  io_device.end_addr     = 0x38B;
  io_device.irq          = EMU_NO_IRQ;
  io_device.fd           = -1;
  if (port_register_handler(io_device, 0) != 0) {
    S_printf("ADLIB: Error registering port handler\n");
    SB_info.version = SB_NONE;
  }

  (void) FM_driver_init();
}

static void mpu401_init(void)
{
  emu_iodev_t  io_device;
  
  S_printf ("MPU401: MPU-401 Initialisation\n");

  /* This is the MPU-401 */
  io_device.read_portb   = mpu401_io_read;
  io_device.write_portb  = mpu401_io_write;
  io_device.read_portw   = NULL;
  io_device.write_portw  = NULL;
  io_device.read_portd   = NULL;
  io_device.write_portd  = NULL;
  io_device.handler_name = "Midi Emulation";
  io_device.start_addr   = config.mpu401_base;
  io_device.end_addr     = config.mpu401_base + 0x001;
  io_device.irq          = EMU_NO_IRQ;
  io_device.fd           = -1;
  if (port_register_handler(io_device, 0) != 0) {
    S_printf("MPU-401: Error registering port handler\n");
    SB_info.version = SB_NONE;
  }

  S_printf ("MPU401: MPU-401 Initialisation - Base 0x%03x \n", 
	    config.mpu401_base);

  mpu401_info.isdata = 0;

  (void) MPU_driver_init();
}


/*
 * Sound Reset
 * ===========
 */

void sound_reset(void)
{
  if (config.sound) {
    sb_reset();
    fm_reset();
    mpu401_reset();
  }
}

static void sb_reset (void)
{
  S_printf ("SB: Resetting SB\n");

  sb_detect();			/* check if noone occupied /dev/dsp */

  if (SB_info.version != SB_NONE) {

    sb_deactivate_irq(SB_IRQ_PEND);
    SB_info.irq.active = 0;

    sb_disable_speaker();
    dsp_clear_output ();
    SB_dsp.pause_state = 0;

    SB_dsp.dma_mode = 0;
    sb_is_running = 0;
    SB_dsp.empty_state = 0;
    SB_dsp.num_parameters = 0;
    SB_dsp.bytes_left = 0;
    dma_pending = 0;
    byte_skipped = 0;
    SB_dsp.stereo = 0;
    SB_dsp.length = 0;
    dsp_block_size = 0;
    m_E2Value = 0xaa;
    m_E2Count = 0;
    E2_Active = 0;
    DSP_busy_hack = 2;
    SB_dsp.ready = 0x7f;
/* the following must not be zeroed out */
#if 0
    SB_info.mixer_index = 0;
    SB_dsp.sample_rate = 0;
    SB_dsp.time_constant = 0;
#endif
    SB_driver_reset();
  }
  else {
    SB_dsp.ready = 0xff;
    SB_dsp.data = 0xff;
  }
}

static void fm_reset (void)
{
  /* extern struct adlib_info_t adlib_info; - For reference - AM */
  extern struct adlib_timer_t adlib_timers[2];

  S_printf ("SB: Resetting FM\n");

  adlib_timers[0].enabled = 0;
  adlib_timers[1].enabled = 0;

  FM_driver_reset();
}

static void mpu401_reset (void)
{
	S_printf("MPU401: Resetting MPU-401\n");

  MPU_driver_reset();
}

static void sb_write_DAC (int bits, __u8 value)
{
  S_printf ("SB: Direct DAC write (%u bits)\n", bits);
  
  if (SB_driver.DAC_write == NULL) {
    S_printf ("SB: Required function 'DAC_write' not provided.\n");
  }
  else {
    (*SB_driver.DAC_write)(bits, value);
  }
}


/*
 * Miscellaneous Functions
 */

void sb_enable_speaker (void)
{
  if (SB_info.speaker) {
    S_printf ("SB: Speaker already Enabled. Ignoring\n");
    return;
  }

  S_printf ("SB: Enabling Speaker\n");

  if (SB_driver.speaker_on == NULL) {
    S_printf ("SB: Required function 'speaker_on' not provided.\n");
  }
  else {
    (*SB_driver.speaker_on)();
  }

  SB_info.speaker = 1;
}

void sb_disable_speaker(void)
{
  S_printf ("SB: Disabling Speaker\n");

  if (SB_driver.speaker_off == NULL) {
    S_printf ("SB: Required function 'speaker_off' not provided.\n");
  }
  else {
    (*SB_driver.speaker_off)();
  }

  SB_info.speaker = 0;
}

static __u8 sb_read_mixer(__u8 ch)
{
  S_printf ("SB: Read mixer setting on channel %u.\n", ch);

  if (SB_driver.read_mixer == NULL) {
    S_printf ("SB: Required function 'read_mixer' not provided.\n");
    return 0xFF;
  }
  else {
    return (*SB_driver.read_mixer)(ch);
  }
}

static void sb_write_mixer (int ch, __u8 value)
{
  S_printf ("SB: Write mixer setting on channel %u.\n", ch);

  if (SB_driver.write_mixer == NULL) {
    S_printf ("SB: Required function 'write_mixer' not provided.\n");
  }
  else {
    (*SB_driver.write_mixer)(ch, value);
  }
}


/*
 * BEWARE: Experimental Code !
 *
 */

void sb_controller(void) {

  if (sb_is_running & FM_TIMER_RUN)
    sb_update_timers ();

  if (sb_is_running & DSP_OUTPUT_RUN)
    sb_check_complete ();
}

/* Blatant rip-off of serial_update_timers */
void sb_update_timers () {
  static hitimer_t oldtp = 0;		/* Timer value from last call */
  hitimer_t tp;				/* Current timer value */
  unsigned long elapsed;		/* No of 80useconds elapsed */
  Bit8u current_value;
  Bit16u int08_irq;
  /* extern struct adlib_info_t adlib_info; - For reference - AM */
  extern struct adlib_timer_t adlib_timers[2];

  if ( (adlib_timers[0].enabled != 1) 
       && (adlib_timers[1].enabled != 1) ) {

    /* 
     * We only make it here if both of the timers have been turned off 
     * individually, rather than using the reset. We turn this off in the
     * 'sb_is_running' flags - AM
     */
    sb_is_running &= (~FM_TIMER_RUN);

    return;
  }

  /* Get system time.  PLEASE DONT CHANGE THIS LINE, unless you can 
   * _guarantee_ that the substitute/stored timer value _is_ up to date 
   * at _this_ instant!  (i.e: vm86s exit time did not not work well)
   */
  tp = GETusTIME(0);
  if (oldtp==0)	oldtp=tp;
  /* compute the number of 80us(12500 Hz) since last timer update */
  elapsed = (tp - oldtp) / 80;

  /* Save the old timer values for next time */
  oldtp += elapsed * 80;

  int08_irq = pic_irq_list[0x08];

  if (adlib_timers[0].enabled == 1) {
    current_value = adlib_timers[0].counter;
    adlib_timers[0].counter += elapsed;
    S_printf ("Adlib: timer1 %d\n", adlib_timers[0].counter);
    if (current_value > adlib_timers[0].counter) {
      S_printf ("Adlib: timer1 has expired \n");
      adlib_timers[0].expired = 1;
      pic_request(int08_irq);    
    }
  }
  if (adlib_timers[1].enabled == 1) {
    current_value = adlib_timers[1].counter;
    S_printf ("Adlib: timer2 %d\n", adlib_timers[1].counter);
    adlib_timers[1].counter += (elapsed / 4);
    if (current_value > adlib_timers[1].counter) {
      S_printf ("Adlib: timer2 has expired \n");
      adlib_timers[1].expired = 1;
      pic_request(int08_irq);    
    }
  }
}


/*
 * IRQ Support
 * ===========
 */


/* 
 * This code was originally by Michael Karcher, but has had a number of
 * assumptions cleaned up - AM
 */

static void sb_activate_irq (int type)
{
    S_printf ("SB: Activating irq type %d\n",type);
    if ((SB_info.irq.pending | SB_info.irq.active) & type) {
      S_printf("SB: Warning: Interrupt already active!\n");
      return;
    }

    /* On a real SB-board, all three sources use the same IRQ-number. -MK */
    /* So ? Just means we are more flexible. - AM */

    switch(type)
    {
        case SB_IRQ_8BIT:
          pic_request(SB_info.irq.irq8);
          break;
        case SB_IRQ_16BIT:
          pic_request(SB_info.irq.irq16);
          break;
        case SB_IRQ_MIDI:
          pic_request(SB_info.irq.midi);
          break;
        /* Any more ? - AM */
        /* On an SB16: No - MK */
        default:
          S_printf("SB: ERROR! Wrong IRQ-type passed to activate_irq!\n");
          break;
    }
    SB_info.irq.pending |= type;
}

static void sb_deactivate_irq (int type)
{
    if(SB_dsp.empty_state & IRQ_AT_EMPTY) {
      S_printf("SB: Untriggering scheduled IRQ\n");
      SB_dsp.empty_state &= ~IRQ_AT_EMPTY;
    }
    if(!(SB_info.irq.pending & type)) {
      return;
    }
    S_printf ("SB: Warning: Deactivating irq type %d\n",type);

    /* On a real SB-board, all three sources use the same IRQ-number. -MK */
    /* So ? Just means we are more flexible. - AM */

    if(SB_info.irq.pending & type & SB_IRQ_8BIT)
        pic_untrigger(SB_info.irq.irq8);
    if(SB_info.irq.pending & type & SB_IRQ_16BIT)
        pic_untrigger(SB_info.irq.irq16);
    if(SB_info.irq.pending & type & SB_IRQ_MIDI)
        pic_untrigger(SB_info.irq.midi);
        /* Any more ? - AM */
        /* On an SB16: No - MK */
    SB_info.irq.pending &= ~type;
}
