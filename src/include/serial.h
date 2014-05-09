/*
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* DANG_BEGIN_MODULE
 *
 * REMARK
 * serial.h: Include file for port data array for DOSEMU serial.
 * Please send bug reports and bugfixes to marky@magmacom.com
 * Please read the files in the 'serial' subdirectory for more info.
 * /REMARK
 *
 * This module is maintained by Mark Rejhon at these Email addresses:
 *      marky@magmacom.com
 *      ag115@freenet.carleton.ca
 *
 * COPYRIGHTS
 * ~~~~~~~~~~
 *   Copyright (C) 1995 by Mark Rejhon
 *
 *   All of this code is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License as
 *   published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 * DANG_END_MODULE
 */

#ifndef SERIAL_H
#define SERIAL_H

#include <termios.h>
#include "extern.h"

#define MAX_SER 16
#define NUM_COMS 4

extern int no_local_video; /* used by virtual port code */

typedef struct {
  				/*   MAIN VARIABLES  */
  char *dev;			/* String to hold path to device file */
  int real_comport;		/* The actual COMx port number. 0 for invalid */
  ioport_t base_port;		/* Base port address handled by device */
  int irq;			/* IRQ line handled by device */
  boolean virtual;		/* virtual modem */
  boolean pseudo;		/* pseudo-tty is used */
  boolean low_latency;		/* set low_latency mode */
  boolean mouse;		/* Flag to turn on mouse sharing features */
  int system_rtscts;		/* Flag: emulate RTS or let system handle it */
  ioport_t dmx_port;
  Bit8u dmx_mask;
  int dmx_shift;
  int dmx_val:1;
} serial_t;

EXTERN serial_t com_cfg[MAX_SER];

extern int int14(void);
extern void serial_run(void);
extern int do_serial_in(int, ioport_t);
extern int do_serial_out(int, ioport_t, int);
extern void serial_helper(void);
extern void child_close_mouse(void);
extern void child_open_mouse(void);

#endif /* SERIAL_H */
