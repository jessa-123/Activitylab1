/* Modified for DOSEMU's internal mouse driver support
 * by Alan Hourihane (alanh@metro.co.uk, alanh@sound.demon.co.uk)
 * (29/4/94)
 */

/*
 *
 * Copyright 1990,91 by Thomas Roell, Dinkelscherben, Germany.
 * Copyright 1993 by David Dawes <dawes@physics.su.oz.au>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the names of Thomas Roell and David Dawes not be
 * used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  Thomas Roell
 * and David Dawes makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *
 * THOMAS ROELL AND DAVID DAWES DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THOMAS ROELL OR DAVID DAWES BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include <unistd.h>
#include <sys/types.h>
#include <termios.h>
#include <errno.h>

#include "emu.h"
#include "mouse.h"

void mouse_move(void), mouse_lb(void), mouse_rb(void);
void DOSEMUSetMouseSpeed();

/*
 * DOSEMUSetupMouse --
 *	Sets up the mouse parameters
 */

void
DOSEMUSetupMouse()
{
      if (mice->type == MOUSE_MOUSEMAN)
        {
          DOSEMUSetMouseSpeed(1200, 1200, mice->flags);
          write(mice->fd, "*X", 2);
          DOSEMUSetMouseSpeed(1200, mice->baudRate, mice->flags);
        }
      else if (mice->type != MOUSE_BUSMOUSE && mice->type != MOUSE_PS2) 
	{
	  DOSEMUSetMouseSpeed(9600, mice->baudRate, mice->flags);
	  DOSEMUSetMouseSpeed(4800, mice->baudRate, mice->flags);
	  DOSEMUSetMouseSpeed(2400, mice->baudRate, mice->flags);
	  DOSEMUSetMouseSpeed(1200, mice->baudRate, mice->flags);

	  if (mice->type == MOUSE_LOGITECH)
	    {
	      write(mice->fd, "S", 1);
	      DOSEMUSetMouseSpeed(mice->baudRate, mice->baudRate, mice->flags);
	    }

	  if (mice->type == MOUSE_HITACHI)
	  {
	    char speedcmd;

	    write(mice->fd, "z8", 2);	/* Set Parity = "NONE" */
	    usleep(50000);
	    write(mice->fd, "zb", 2);	/* Set Format = "Binary" */
	    usleep(50000);
	    write(mice->fd, "@", 1);	/* Set Report Mode = "Stream" */
	    usleep(50000);
	    write(mice->fd, "R", 1);	/* Set Output Rate = "45 rps" */
	    usleep(50000);
	    write(mice->fd, "I\x20", 2);	/* Set Incrememtal Mode "20" */
	    usleep(50000);
	    write(mice->fd, "E", 1);	/* Set Data Type = "Relative */
	    usleep(50000);

	    /* These sample rates translate to 'lines per inch' on the Hitachi
	       tablet */
	    if      (mice->sampleRate <=   40) speedcmd = 'g';
	    else if (mice->sampleRate <=  100) speedcmd = 'd';
	    else if (mice->sampleRate <=  200) speedcmd = 'e';
	    else if (mice->sampleRate <=  500) speedcmd = 'h';
	    else if (mice->sampleRate <= 1000) speedcmd = 'j';
	    else                                  speedcmd = 'd';
	    write(mice->fd, &speedcmd, 1);
	    usleep(50000);

	    write(mice->fd, "\021", 1);	/* Resume DATA output */
	  }
	  else
	  {
	    if      (mice->sampleRate <=   0)  write(mice->fd, "O", 1);
	    else if (mice->sampleRate <=  15)  write(mice->fd, "J", 1);
	    else if (mice->sampleRate <=  27)  write(mice->fd, "K", 1);
	    else if (mice->sampleRate <=  42)  write(mice->fd, "L", 1);
	    else if (mice->sampleRate <=  60)  write(mice->fd, "R", 1);
	    else if (mice->sampleRate <=  85)  write(mice->fd, "M", 1);
	    else if (mice->sampleRate <= 125)  write(mice->fd, "Q", 1);
	    else                                  write(mice->fd, "N", 1);
	  }
        }

#ifdef CLEARDTR_SUPPORT
      if (mice->type == MOUSE_MOUSESYSTEMS && (mice->cleardtr))
        {
          int val = TIOCM_DTR;
          ioctl(mice->fd, TIOCMBIC, &val);
        }
#if 0     /* Jochen 05.05.94 */
/* Not used for my 3 button mouse */
      if (mice->type == MOUSE_MOUSESYSTEMS && (mice->flags & MF_CLEAR_RTS))
        {
          int val = TIOCM_RTS;
          ioctl(mice->fd, TIOCMBIC, &val);
        }
#endif
#endif
}
 
void
DOSEMUMouseProtocol(rBuf, nBytes)
     unsigned char *rBuf;
     int nBytes;
{
  int                  i, buttons=0, dx=0, dy=0;
  static int           pBufP = 0;
  static unsigned char pBuf[8];

  static unsigned char proto[8][5] = {
    /*  hd_mask hd_id   dp_mask dp_id   nobytes */
    { 	0x40,	0x40,	0x40,	0x00,	3 	},  /* MicroSoft */
    {	0xf8,	0x80,	0x00,	0x00,	5	},  /* MouseSystems */
    {	0xe0,	0x80,	0x80,	0x00,	3	},  /* MMSeries */
    {	0xe0,	0x80,	0x80,	0x00,	3	},  /* Logitech */
    {	0xf8,	0x80,	0x00,	0x00,	5	},  /* BusMouse */
    { 	0x40,	0x40,	0x40,	0x00,	3 	},  /* MouseMan
                                                       [CHRIS-211092] */
    {	0xc0,	0x00,	0x00,	0x00,	3	},  /* PS/2 mouse */
    {	0xe0,	0x80,	0x80,	0x00,	3	},  /* MM_HitTablet */
  };
  
  for ( i=0; i < nBytes; i++) {
    if (pBufP != 0 && mice->type != MOUSE_PS2 &&
	((rBuf[i] & proto[mice->type][2]) != proto[mice->type][3]
	 || rBuf[i] == 0x80))
      {
	pBufP = 0;          /* skip package */
      }

    if (pBufP == 0 &&
	(rBuf[i] & proto[mice->type][0]) != proto[mice->type][1])
      {
	if ((mice->type == MOUSE_MICROSOFT || mice->type == MOUSE_MOUSEMAN)
          && (char)(rBuf[i] & ~0x23) == 0)
	  {
	    buttons = ((int)(rBuf[i] & 0x20) >> 4)
	      | (mice->lastButtons & 0x05);
/*
	    DOSEMUPostMseEvent(buttons, 0, 0);
*/
	  }

	continue;            /* skip package */
      }


    pBuf[pBufP++] = rBuf[i];
    if (pBufP != proto[mice->type][4]) continue;

    /*
     * assembly full package
     */
    switch(mice->type) {
      
    case MOUSE_MOUSEMAN:	    /* MouseMan / TrackMan   [CHRIS-211092] */
    case MOUSE_MICROSOFT:              /* Microsoft */
      if (mice->chordMiddle)
	buttons = (((int) pBuf[0] & 0x30) == 0x30) ? 2 :
		  ((int)(pBuf[0] & 0x20) >> 3)
		  | ((int)(pBuf[0] & 0x10) >> 4);
      else
	buttons = (mice->lastButtons & 2)
		  | ((int)(pBuf[0] & 0x20) >> 3)
		  | ((int)(pBuf[0] & 0x10) >> 4);
      dx = (char)(((pBuf[0] & 0x03) << 6) | (pBuf[1] & 0x3F));
      dy = (char)(((pBuf[0] & 0x0C) << 4) | (pBuf[2] & 0x3F));
      break;
      
    case MOUSE_MOUSESYSTEMS:             /* Mouse Systems Corp */
      buttons = (~pBuf[0]) & 0x07;
      dx =    (char)(pBuf[1]) + (char)(pBuf[3]);
      dy = - ((char)(pBuf[2]) + (char)(pBuf[4]));
      break;
      
    case MOUSE_HITACHI:           /* MM_HitTablet */
      buttons = pBuf[0] & 0x07;
      if (buttons != 0)
        buttons = 1 << (buttons - 1);
      dx = (pBuf[0] & 0x10) ?   pBuf[1] : - pBuf[1];
      dy = (pBuf[0] & 0x08) ? - pBuf[2] :   pBuf[2];
      break;

    case MOUSE_MMSERIES:              /* MM Series */
    case MOUSE_LOGITECH:            /* Logitech Mice */
      buttons = pBuf[0] & 0x07;
      dx = (pBuf[0] & 0x10) ?   pBuf[1] : - pBuf[1];
      dy = (pBuf[0] & 0x08) ? - pBuf[2] :   pBuf[2];
      break;
      
    case MOUSE_BUSMOUSE:              /* BusMouse */
      buttons = (~pBuf[0]) & 0x07;
      dx =   (char)pBuf[1];
      dy = - (char)pBuf[2];
      break;

    case MOUSE_PS2:		    /* PS/2 mouse */
      buttons = (pBuf[0] & 0x04) >> 1 |       /* Middle */
	        (pBuf[0] & 0x02) >> 1 |       /* Right */
		(pBuf[0] & 0x01) << 2;        /* Left */
      dx = (pBuf[0] & 0x10) ?    pBuf[1]-256  :  pBuf[1];
      dy = (pBuf[0] & 0x20) ?  -(pBuf[2]-256) : -pBuf[2];
      break;
    }

/*
 *  Here for 3 button emulation, if needed ? 
    DOSEMUPostMseEvent(buttons, dx, dy);
*/

    mouse.x = mouse.x + dx;
    mouse.y = mouse.y + dy;
    mouse.cx = mouse.x / 8;
    mouse.cy = mouse.y / 8;
    if (dx || dy) mouse_move();
    mouse.oldlbutton = mouse.lbutton;
    mouse.oldrbutton = mouse.rbutton;
    mouse.lbutton = buttons & 0x04;
    mouse.mbutton = buttons & 0x02;
    mouse.rbutton = buttons & 0x01;
    if (mouse.oldlbutton != mouse.lbutton)
    mouse_lb();
/*
 *  mouse_mb();         When we have 3 button support
 */
    if (mouse.oldrbutton != mouse.rbutton)
    mouse_rb();   

    pBufP = 0;
  }
}

void DOSEMUSetMouseSpeed(old, new, cflag)
int old;
int new;
unsigned cflag;
{
	struct termios tty;
	char *c;

	if (tcgetattr(mice->fd, &tty) < 0)
	{
		m_printf("MOUSE: Unable to get status of mouse. Mouse may not function properly.\n");
	}

	tty.c_iflag = IGNBRK | IGNPAR;
	tty.c_oflag = 0;
	tty.c_lflag = 0;
	tty.c_cflag = (tcflag_t)cflag;
	tty.c_cc[VTIME] = 0;
	tty.c_cc[VMIN] = 1;

	switch (old)
	{
	case 9600:
		cfsetispeed(&tty, B9600);
		cfsetospeed(&tty, B9600);
		break;
	case 4800:
		cfsetispeed(&tty, B4800);
		cfsetospeed(&tty, B4800);
		break;
	case 2400:
		cfsetispeed(&tty, B2400);
		cfsetospeed(&tty, B2400);
		break;
	case 1200:
	default:
		cfsetispeed(&tty, B1200);
		cfsetospeed(&tty, B1200);
	}

	if (tcsetattr(mice->fd, TCSADRAIN, &tty) < 0)
	{
		m_printf("MOUSE: Unable to set mouse attributes. Mouse may not function properly.\n");
	}

	switch (new)
	{
	case 9600:
		c = "*q";
		cfsetispeed(&tty, B9600);
		cfsetospeed(&tty, B9600);
		break;
	case 4800:
		c = "*p";
		cfsetispeed(&tty, B4800);
		cfsetospeed(&tty, B4800);
		break;
	case 2400:
		c = "*o";
		cfsetispeed(&tty, B2400);
		cfsetospeed(&tty, B2400);
		break;
	case 1200:
	default:
		c = "*n";
		cfsetispeed(&tty, B1200);
		cfsetospeed(&tty, B1200);
	}

	if (write(mice->fd, c, 2) != 2)
	{
		m_printf("MOUSE: Unable to write to mouse fd. Mouse may not function properly.\n");
	}
	usleep(100000);

	if (tcsetattr(mice->fd, TCSADRAIN, &tty) < 0)
	{
		m_printf("MOUSE: Unable to set mouse attributes. Mouse may not function properly.\n");
	}
}

void DOSEMUMouseEvents()
{
	unsigned char rBuf[64];
	int nBytes;

	nBytes = RPT_SYSCALL(read(mice->fd, (char *)rBuf, sizeof(rBuf)));
	DOSEMUMouseProtocol(rBuf, nBytes);
}

