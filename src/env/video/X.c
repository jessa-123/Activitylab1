/* 
 * (C) Copyright 1992, ..., 2005 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* 
 * DANG_BEGIN_MODULE
 *
 * REMARK
 * This module contains the video interface for the X Window 
 * System. It has mouse and selection 'cut' support.
 *
 * /REMARK
 * DANG_END_MODULE
 *
 * DANG_BEGIN_CHANGELOG
 *
 * $Id$
 *
 * 951205: bon@elektron.ikp.physik.th-darmstadt.de
 *   Merged in again the selection stuff. Hope introduce hidden bugs 
 *        are removed!  
 *
 *         TODO: highlighting should be smarter and either discard area
 *               or follow ir when scrolling (text-mode
 *
 * 5/24/95, Started to get the graphics modes to work
 * Erik Mouw (J.A.K.Mouw@et.tudelft.nl) 
 * and Arjan Filius (I.A.Filius@et.tudelft.nl)
 *
 * changed:
 *  X_setmode()
 *  X_redraw_screen()
 *  X_update_screen()
 *  set_mouse_position()
 * in video/X.c
 *
 * and:
 *  set_video_mode()
 * in video/int10.c
 * int10()
 *
 * added:
 *  graphics_cmap_init()
 *
 * Now dosemu can work in graphics mode with X, but at the moment only in mode
 * 0x13 and very slow.
 *
 * 1995-08-30: Merged with code for pasting, removed some restrictions 
 * from Pasi's code: cut and paste and x_mouse, 
 * (bon@elektron.ikp.physik.th-darmstadt.de)
 *
 * 1995-08-22: Lots of cleaning and rewriting, more comments, MappingNotify 
 * event support, selection support (cut only), fixed some cursor redrawing 
 * bugs. -- Pasi Eronen (pe@iki.fi)
 *
 *
 * 1996/04/29: Added the ability to use MIT-SHM X extensions to speed
 * up graphics handling... compile-time option.  Typically halves system
 * overhead for local graphics. -- Adam D. Moss (aspirin@tigerden.com) (adm)
 *
 * 1996/05/03: Graphics are MUCH faster now due to changes in vgaemu.c. --adm
 *
 * 1996/05/05: Re-architected the process of delivering changes from
 * vgaemu.c to the client (X.c).  This will allow further optimization
 * in the future (I hope). --adm
 *
 * 1996/05/06: Began work on shared-colourmap and mismatched-colourdepth
 * graphics support. --adm
 *
 * 1996/05/20: More work on colourmap, bugfixes, speedups, and better
 * mouse support in graphics modes. --adm
 *
 * 1996/08/18: Make xdos work with TrueColor displays (again?) -- Thomas Pundt
 * (pundtt@math.uni-muenster.de)
 *
 * 1997/01/19: Made mouse functionable for win31-in-xdos (hiding X-cursor,
 * calibrating win31 cursor) -- Hans
 *
 * 1997/02/03: Adapted Steffen Winterfeld's (graphics) true color patch from
 * 0.63.1.98 to 0.64.3.2.
 * The code now looks a bit kludgy, because it was written for for the .98,
 * but it works. Scaling x2 is only set for 320x200.  -- Hans
 *
 * 1997/03/03: Added code to turn off MIT-SHM for network connections.
 * -- sw (Steffen Winterfeldt <wfeldt@suse.de>)
 *
 * 1997/04/21: Changed the interface to vgaemu - basically we do
 * all image conversions in remap.c now. Added & enhanced support
 * for 8/15/16/24/32 bit displays (you can resize the graphics
 * window now).
 * Palette updates in text-modes work; the code is not
 * perfect yet (no screen update is done), so the change will
 * take effect only after some time...
 * -- sw (Steffen Winterfeldt <wfeldt@suse.de>)
 *
 * 1997/06/06: Fixed the code that was supposed to turn off MIT-SHM for
 * network connections. Thanks to Leonid V. Kalmankin <leonid@cs.msu.su>
 * for finding the bug and testing the fix.
 * -- sw (Steffen Winterfeldt <wfeldt@suse.de>)
 *              
 * 1997/06/15: Added gamma correction for graphics modes.
 * -- sw
 *
 * 1998/02/22: Rework of mouse support in graphics modes; put in some
 * code to enable mouse grabbing. Can be activated via Ctrl+Alt+<some configurable key>;
 * default is "Home".
 * -- sw
 *
 * 1998/03/08: Further work at mouse support in graphics modes.
 * Added config capabilities via DOS tool (X_change_config).
 * -- sw
 *
 * 1998/03/15: Another try to get rid of RESIZE_KLUGE. Fixed continuous
 * X-cursor redefinition in graphics modes. Worked on color allocation.
 * -- sw
 *
 * 1998/04/05: We are using backing store now in text modes.
 * -- sw
 *
 * 1998/05/28: some re-arangements of the mouse code.
 * -- EB
 *
 * 1998/09/16: Continued work on color allocation.
 * Added graphics support for 16 color X servers (e.g. XF86_VGA16).
 * Text mode now works on all X servers incl. 16 color & mono servers.
 * -- sw
 *
 * 1998/09/20: Removed quite a lot of DAC/color init code. Were (hopefully)
 * all unnecessary. :-)
 * -- sw
 *
 * 1998/11/01: Added so far unsupported video modes (CGA, 1 bit & 2 bit VGA).
 * -- sw
 *
 * 1998/12/12: Palette/font changes in text modes improved.
 * -- sw
 *
 * 1999/01/05: Splitted X_update_screen() into two separate parts.
 * -- sw
 *
 * 1998/11/07: Updated Steffen's code to newest X.c :
 *
 *  * 1998/08/09: Started to add code to support KDE/Qt frontend.
 *  * Look for DOSEMU_WINDOW_ID.
 *  * -- sw
 *
 * -- Antonio Larrosa (antlarr@arrakis.es)
 *
 * 2000/05/31: Cleaned up mouse handling with help of Bart Oldeman.
 *  -- Eric Biederman 
 *
 * 2003/02/02: Implemented full-screen mode. Thanks to
 *             Michael Graffam <mgraffam@idsi.net> for his XF86 VidMode
 *             Extension example.
 *  -- Bart Oldeman
 *
 * 2002/11/30: Began "VRAM" user font processing (X_draw_string...)
 *  Probably not nicely integrated into the remapper -> no resizing!
 *  Experimental FONT_LAB define available, too, to override int 0x43
 *  in graphics mode. Bonus: Colorable OVERSCAN area in text mode.
 *  Changed int10, vgaemu, seqemu, vgaemu.h, attremu, X. Sigh!
 *  TODO: Code clean-up... And probably some optimizations.
 *  TODO: Use video RAM instead of vga_font_ram -> more compatibility!
 *  Introduded X_textscroll (see also vgaemu.h, int10.c).
 *  -- Eric (eric@coli.uni-sb.de) (activate: #define FONT_LAB2 1)
 *
 * DANG_END_CHANGELOG
 */


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * some configurable options
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Debug level for the X interface.
 * 0 - normal / 1 - useful / 2 - too much
 */
#define DEBUG_X		0

/* show X mouse in graphics modes */
#undef DEBUG_MOUSE_POS

/* define if you wish to have a visual indication of the area being updated */
#undef DEBUG_SHOW_UPDATE_AREA

/* use backing store in text modes */
#define X_USE_BACKING_STORE

/* Do you want speaker support in X? */
#define CONFIG_X_SPEAKER 1

/* not yet -- sw */
#undef HAVE_DGA

#define CONFIG_X_MOUSE 1


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define x_msg(x...) X_printf("X: " x)

#if DEBUG_X >= 1
#define x_deb(x...) X_printf("X: " x)
#else
#define x_deb(x...)
#endif

#if DEBUG_X >= 2
#define x_deb2(x...) X_printf("X: " x)
#else
#define x_deb2(x...)
#endif


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#include <config.h>
#include <stdio.h>
#include <termios.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <sys/mman.h>           /* root@sjoerd:for mprotect*/

#ifdef HAVE_XVIDMODE
#include <X11/extensions/xf86vmode.h>
#endif

#include "emu.h"
#include "timers.h"
#include "bios.h"
#include "video.h"
#include "memory.h"
#include "remap.h"
#include "vgaemu.h"
#include "vgatext.h"
#include "render.h"
#include "keyb_server.h"
#include "X.h"
#include "dosemu_config.h"

#ifdef HAVE_MITSHM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#endif

#ifdef HAVE_DGA
#include <X11/extensions/xf86dga.h>
#endif

#ifdef HAVE_XKB
#include <X11/XKBlib.h>
#endif

#if CONFIG_X_MOUSE
#include "mouse.h"
#include <time.h>
#endif

#if CONFIG_SELECTION
#define CONFIG_X_SELECTION 1
#include "screen.h"
#endif

#if CONFIG_X_SPEAKER
#include "speaker.h"
#endif


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
typedef struct { unsigned char r, g, b; } c_cube;


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef HAVE_UNICODE_TRANSLATION
static const u_char dos_to_latin[] = {
   0xc7, 0xfc, 0xe9, 0xe2, 0xe4, 0xe0, 0xe5, 0xe7,  /* 80-87 */ 
   0xea, 0xeb, 0xe8, 0xef, 0xee, 0xec, 0xc4, 0xc5,  /* 88-8F */
   0xc9, 0xe6, 0xc6, 0xf4, 0xf6, 0xf2, 0xfb, 0xf9,  /* 90-97 */
   0xff, 0xd6, 0xdc, 0xa2, 0xa3, 0xa5, 0x00, 0x00,  /* 98-9F */
   0xe1, 0xed, 0xf3, 0xfa, 0xf1, 0xd1, 0xaa, 0xba,  /* A0-A7 */
   0xbf, 0x00, 0xac, 0xbd, 0xbc, 0xa1, 0xab, 0xbb,  /* A8-AF */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* B0-B7 */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* B8-BF */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* C0-C7 */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* C8-CF */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* D0-D7 */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* D8-DF */
   0x00, 0xdf, 0x00, 0x00, 0x00, 0x00, 0xb5, 0x00,  /* E0-E7 */
   0x00, 0x00, 0x00, 0xf0, 0x00, 0xd8, 0x00, 0x00,  /* E8-EF */
   0x00, 0xb1, 0x00, 0x00, 0x00, 0x00, 0xf7, 0x00,  /* F0-F7 */
   0xb0, 0xb7, 0x00, 0x00, 0xb3, 0xb2, 0x00, 0x00   /* F8-FF */
};  

static const u_char dos_to_latin1[] = {
   0xc7, 0xfc, 0xe9, 0xe2, 0xe4, 0xe0, 0xe5, 0xe7,  /* 80-87 */
   0xea, 0xeb, 0xe8, 0xef, 0xee, 0xec, 0xc4, 0xc5,  /* 88-8F */
   0xc9, 0xe6, 0xc6, 0xf4, 0xf6, 0xf2, 0xfb, 0xf9,  /* 90-97 */
   0xff, 0xd6, 0xdc, 0xa2, 0xa3, 0xd8, 0xd7, 0x00,  /* 98-9F */
   0xe1, 0xed, 0xf3, 0xfa, 0xf1, 0xd1, 0xaa, 0xba,  /* A0-A7 */
   0xbf, 0xae, 0xac, 0xbd, 0xbc, 0xa1, 0xab, 0xbb,  /* A8-AF */
   0x00, 0x00, 0x00, 0x00, 0x00, 0xc1, 0xc2, 0xc0,  /* B0-B7 */
   0xa9, 0x00, 0x00, 0x00, 0x00, 0xa2, 0xa5, 0xac,  /* B8-BF */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe3, 0xc3,  /* C0-C7 */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa4,  /* C8-CF */
   0xf0, 0xd0, 0xca, 0xcb, 0xc8, 0x00, 0xcd, 0xce,  /* D0-D7 */
   0xcf, 0x00, 0x00, 0x00, 0x00, 0xa6, 0xcc, 0x00,  /* D8-DF */
   0xd3, 0xdf, 0xd4, 0xd2, 0xf5, 0xe5, 0xb5, 0xdc,  /* E0-E7 */
   0xfc, 0xda, 0xdb, 0xd9, 0xfd, 0xdd, 0xad, 0xb4,  /* E8-EF */
   0xad, 0xb1, 0x00, 0xbe, 0xb6, 0xa7, 0xf7, 0xb8,  /* F0-F7 */
   0xb0, 0xa8, 0xb7, 0xb9, 0xb3, 0xb2, 0x00, 0xa0   /* F8-FF */
};

static const u_char dos_to_latin2[] = {
   0xc7, 0xfc, 0xe9, 0xe2, 0xe4, 0xf9, 0xe6, 0xe7,  /* 80-87 */
   0xb3, 0xeb, 0xf5, 0xd5, 0xee, 0xac, 0xc4, 0xc6,  /* 88-8F */
   0xc9, 0xc5, 0xe5, 0xf4, 0xf6, 0xa5, 0xb5, 0xa6,  /* 90-97 */
   0xb6, 0xd6, 0xdc, 0xab, 0xbb, 0xa3, 0xd7, 0xe8,  /* 98-9F */
   0xe1, 0xed, 0xf3, 0xfa, 0xa1, 0xb1, 0xae, 0xbe,  /* A0-A7 */
   0xca, 0xea, 0x00, 0xbc, 0xc8, 0xba, 0xab, 0xbb,  /* A8-AF */
   0x00, 0x00, 0x00, 0x00, 0x00, 0xc1, 0xc2, 0xcc,  /* B0-B7 */
   0xaa, 0x00, 0x00, 0x00, 0x00, 0xaf, 0xbf, 0x00,  /* B8-BF */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc3, 0xe3,  /* C0-C7 */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa4,  /* C8-CF */
   0xf0, 0xd0, 0xcf, 0xcb, 0xef, 0xd2, 0xcd, 0xce,  /* D0-D7 */
   0xec, 0x00, 0x00, 0x00, 0x00, 0xde, 0xd9, 0x00,  /* D8-DF */
   0xd3, 0xdf, 0xd4, 0xd1, 0xf1, 0xf2, 0xa9, 0xb9,  /* E0-E7 */
   0xc0, 0xda, 0xe0, 0xdb, 0xfd, 0xdd, 0xfe, 0xb4,  /* E8-EF */
   0xad, 0xbd, 0xb2, 0xb7, 0xa2, 0xa7, 0xf7, 0xb8,  /* F0-F7 */
   0xb0, 0xa8, 0xff, 0xfb, 0xd8, 0xf8, 0x00, 0xa0   /* F8-FF */
};

static const u_char dos_to_koi8[] = {
  0xe1, 0xe2, 0xf7, 0xe7, 0xe4, 0xe5, 0xf6, 0xfa,
  0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0,
  0xf2, 0xf3, 0xf4, 0xf5, 0xe6, 0xe8, 0xe3, 0xfe,
  0xfb, 0xfd, 0xff, 0xf9, 0xf8, 0xfc, 0xe0, 0xf1,
  0xc1, 0xc2, 0xd7, 0xc7, 0xc4, 0xc5, 0xd6, 0xda,
  0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0,
  0x90, 0x91, 0x92, 0x81, 0x87, 0xb2, 0xb4, 0xa7,
  0xa6, 0xb5, 0xa1, 0xa8, 0xae, 0xad, 0xac, 0x83,
  0x84, 0x89, 0x88, 0x86, 0x80, 0x8a, 0xaf, 0xb0,
  0xab, 0xa5, 0xbb, 0xb8, 0xb1, 0xa0, 0xbe, 0xb9,
  0xba, 0xb6, 0xb7, 0xaa, 0xa9, 0xa2, 0xa4, 0xbd,
  0xbc, 0x85, 0x82, 0x8d, 0x8c, 0x8e, 0x8f, 0x8b,
  0xd2, 0xd3, 0xd4, 0xd5, 0xc6, 0xc8, 0xc3, 0xde,
  0xdb, 0xdd, 0xdf, 0xd9, 0xd8, 0xdc, 0xc0, 0xd1,
  0xb3, 0xa3, 0x99, 0x98, 0x93, 0x9b, 0x9f, 0x97,
  0x9c, 0x95, 0x9e, 0x96, 0xbf, 0x9d, 0x94, 0x9a
};

#endif /* HAVE_UNICODE_TRANSLATION */

static int (*OldXErrorHandler)(Display *, XErrorEvent *) = NULL;

#ifdef HAVE_XKB
int using_xkb = FALSE;
int xkb_event_base = 0;
int xkb_error_base = 0;
#endif

#ifdef HAVE_MITSHM
static int shm_ok = 0;
static int shm_error_base = 0;
static XShmSegmentInfo shminfo;
#endif

#ifdef HAVE_DGA
static int dga_ok = 0;
static int dga_active = 1;
static unsigned char *dga_addr = NULL;
static unsigned dga_scan_len = 0;
static unsigned dga_width = 0;
static unsigned dga_height = 0;
static unsigned dga_bank = 0;
static unsigned dga_mem = 0;
static unsigned _dga_width = 0;
static unsigned _dga_height = 0;
static Window dga_window;
#endif

#ifdef HAVE_XVIDMODE
static int xf86vm_ok = 0;
static int modecount;          
static XF86VidModeModeInfo **vidmode_modes;
#endif

Display *display;		/* used in plugin/?/keyb_X_keycode.c */
static int screen;
static Visual *visual;
static Window rootwindow, mainwindow, parentwindow, normalwindow, fullscreenwindow;
static int toggling_fullscreen;
static Boolean our_window;     	/* Did we create the window? */

static XImage *ximage;		/* Used as a buffer for the X-server */

static Cursor X_standard_cursor;

#if CONFIG_X_MOUSE
static Cursor X_mouse_nocursor;
static int snap_X = 0;
static int mouse_cursor_visible = 0;
static int mouse_really_left_window = 1;
#endif

static GC gc, fullscreengc, normalgc;
static Font vga_font;
static Atom proto_atom = None, delete_atom = None;
static XFontStruct *font = NULL;
static int font_width, font_shift, shift_x, shift_y;

#if CONFIG_X_SELECTION
static u_char *sel_text = NULL;
static Time sel_time;
enum {
  TARGETS_ATOM,
  TIMESTAMP_ATOM,
  COMPOUND_TARGET,
  UTF8_TARGET,
  TEXT_TARGET,
  STRING_TARGET,
  NUM_TARGETS
};
static Atom targets[NUM_TARGETS];
#endif

static Boolean is_mapped = FALSE;

static int cmap_colors = 0;		/* entries in colormaps: {text,graphics}_cmap */
static Colormap text_cmap = 0, graphics_cmap = 0;
static Boolean have_shmap = FALSE;
static unsigned long text_colors[16];
static int text_col_stats[16] = {0};

static ColorSpaceDesc X_csd;
static int have_true_color;
unsigned dac_bits;			/* the bits visible to the dosemu app, not the real number */
static int x_res, y_res;		/* emulated window size in pixels */
static int w_x_res, w_y_res;		/* actual window size */
static int saved_w_x_res, saved_w_y_res;	/* saved normal window size */
static unsigned ximage_bits_per_pixel;
static unsigned ximage_mode;
static vga_emu_update_type veut;

int grab_active = 0, kbd_grab_active = 0;
#if CONFIG_X_MOUSE
static char *grab_keystring = "Home";
static KeySym grab_keysym = NoSymbol;
static int mouse_x = 0, mouse_y = 0;
static int force_grab = 0;
#endif

static int X_map_mode = -1;
static int X_unmap_mode = -1;

static Atom comm_atom = None;
static Boolean kdos_client = FALSE;    	/* started by kdos */


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* general init/destroy functions */
static int X_init(void);
static void X_close(void);
static void X_get_screen_info(void);
#ifdef HAVE_MITSHM
static void X_shm_init(void);
static void X_shm_done(void);
#endif
#ifdef HAVE_DGA
static void X_dga_init(void);
static void X_dga_done(void);
#endif
#ifdef HAVE_XVIDMODE
static void X_xf86vm_init(void);
static void X_xf86vm_done(void);
#endif

static void X_keymap_init(void);

/* error/event handler */
static int NewXErrorHandler(Display *, XErrorEvent *);
static void X_handle_events(void);

/* interface to xmode.exe */
static char X_title_emuname [X_TITLE_EMUNAME_MAXLEN] = {0};
static char X_title_appname [X_TITLE_APPNAME_MAXLEN] = {0};
static int X_change_config(unsigned, void *);	/* modify X config data from DOS */

/* colormap related stuff */
static void graphics_cmap_init(void);
static int MakePrivateColormap(void);
static ColorSpaceDesc MakeSharedColormap(void);
static int try_cube(unsigned long *, c_cube *);

/* palette/color update stuff */
static void refresh_private_palette(void);
static void get_approx_color(XColor *, Colormap, int);
static void X_set_text_palette(DAC_entry col);

/* ximage/drawing related stuff */
static void create_ximage(void);
static void destroy_ximage(void);
static void put_ximage(int, int, unsigned, unsigned);
static void resize_ximage(unsigned, unsigned);

/* video mode set/modify stuff */
static int X_set_videomode(int, int, int);
static void X_resize_text_screen(void);
static void toggle_fullscreen_mode(void);
static void X_vidmode(int w, int h, int *new_width, int *new_height);
static void lock_window_size(unsigned wx_res, unsigned wy_res);

/* screen update/redraw functions */
static void X_reset_redraw_text_screen(void);
static void X_redraw_text_screen(void);
static int X_update_screen(void);
static void set_gc_attr(Bit8u);
static void X_draw_string(int, int, unsigned char *, int, Bit8u);
static void X_draw_line(int x, int y, int len);
static void bitmap_draw_string(int x, int y, unsigned char *text, int len, Bit8u attr);

/* text mode init stuff (font/cursor) */
static void load_text_font(void);
static void load_cursor_shapes(void);
static Cursor create_invisible_cursor(void);

/* text mode cursor manipulation stuff */
static void X_draw_text_cursor(int x, int y, Bit8u attr, int start, int end, Boolean focus);
static void X_update_cursor(void);

#if CONFIG_X_MOUSE
/* mouse related code */
static void set_mouse_position(int, int);
static void set_mouse_buttons(int);
static void toggle_mouse_grab(void);
#endif
static void X_show_mouse_cursor(int yes);
static void X_set_mouse_cursor(int yes, int mx, int my, int x_range, int y_range);

#if CONFIG_X_SELECTION
static int x_to_col(int);
static int y_to_row(int);
static void send_selection(Time, Window, Atom, Atom);
#endif

void kdos_recv_msg(unsigned char *);
void kdos_send_msg(unsigned char *);
void kdos_close_msg(void);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Interface to video emulation.
 * used in env/video/video.c
 */
struct video_system Video_X = 
{
   NULL,
   X_init,         
   X_close,      
   X_set_videomode,      
   X_update_screen,
   X_update_cursor,
   X_change_config,
   X_handle_events
};

static void X_update(void);

struct text_system Text_X =
{
   X_update,
   X_draw_string, 
   X_draw_line,
   X_draw_text_cursor,
   X_set_text_palette,
   X_resize_text_screen
};

struct render_system Render_X =
{
   refresh_private_palette,
   put_ximage,
};

void X_update(void)
{
   XFlush(display);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* utility function for opening a connection and making certain
 * I am either using or not using the X keyboard Extension.
 */
static Display *XKBOpenDisplay(char *display_name)
{
	Display *dpy;
#ifndef HAVE_XKB
	dpy = XOpenDisplay(display_name);
#else /* HAVE_XKB */
	int use_xkb;
	int major_version, minor_version;
	
	using_xkb = FALSE;

	major_version = XkbMajorVersion;
	minor_version = XkbMinorVersion;
	use_xkb = XkbLibraryVersion(&major_version, &minor_version);
	/* If I can't use the keyboard extension make
	 * sure the library doesn't either.
	 */
	XkbIgnoreExtension(!use_xkb);
	dpy = XOpenDisplay(display_name);
	if (dpy == NULL) {
		return NULL;
	}
	if (!use_xkb) {
		return dpy;
	}
	if (!XkbQueryExtension(dpy, NULL, 
			       &xkb_event_base, &xkb_error_base,
			       &major_version, &minor_version)) {
		return dpy;
	}
	using_xkb = TRUE;
#endif
	return dpy;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * DANG_BEGIN_FUNCTION X_init
 *
 * description:
 * Initialize everything X-related.
 *
 * DANG_END_FUNCTION
 */
int X_init()
{
  XGCValues gcv;
  XClassHint xch;
  XSetWindowAttributes attr;
  char *display_name; 
  char *s;
  int i, remap_src_modes;

  X_printf("X: X_init\n");

  co = 80; li = 25;

  /* Open X connection. */
  display_name = config.X_display ? config.X_display : getenv("DISPLAY");
  display = XKBOpenDisplay(display_name);
  if(display == NULL) {
    error("X: Can't open display \"%s\".\n"
    "Either the connection was refused and you do not have enough\n"
    "access rights to connect to your X server or there is\n"
    "something wrong with the contents of your DISPLAY variable.\n"
    "If the connection was refused then please consult your system\n"
    "administator or read the X documentation for a solution\n"
    "(use xauth, xhost, or ssh -X).\n",
        display_name ? display_name : "");
    leavedos(99);
  }

  /* collect some data about our X server; do it before anything else */
  X_get_screen_info();

  OldXErrorHandler = XSetErrorHandler(NewXErrorHandler);

#ifdef HAVE_MITSHM
  X_shm_init();
#endif

#ifdef HAVE_DGA
  X_dga_init();
#endif

#ifdef HAVE_XVIDMODE
  X_xf86vm_init();
#endif
  
  /* see if we find out something useful about our X server... -- sw */
  X_keymap_init();

  text_cmap = DefaultColormap(display, screen);		/* always use the global palette */
  graphics_cmap_init();				/* graphics modes are more sophisticated */

  /* init graphics mode support */
  remap_src_modes = remapper_init(&ximage_mode, ximage_bits_per_pixel,
				  have_true_color, have_shmap);
  if(!remap_src_modes) {
    error("X: No graphics modes supported on this type of screen!\n");
    /* why do we need a blank screen? */
    leavedos(24);
  }

  load_text_font();
  if (font == NULL)
    font_width = 9;

  saved_w_x_res = w_x_res = x_res = co * font_width;
  saved_w_y_res = w_y_res = y_res = li * font_height;

  s = getenv("DOSEMU_WINDOW_ID");
  if(s && (i = strtol(s, NULL, 0)) > 0) {
    mainwindow = i;
    our_window = FALSE;
    kdos_client = TRUE;
    have_focus = TRUE;
    X_printf("X: X_init: using existing window (id = 0x%x)\n", i);
  }
  else {
    XSetWindowAttributes xwa;
    our_window = TRUE;
    kdos_client = FALSE;
    mainwindow = normalwindow = XCreateSimpleWindow(
      display, rootwindow,
      0, 0,			/* Position */
      w_x_res, w_y_res,		/* Size */
      0, 0,			/* Border width & color */
      text_colors[0]		/* Background color */
    );
    xwa.override_redirect = True;
    xwa.background_pixel = text_colors[0];
    fullscreenwindow = XCreateWindow(
      display, rootwindow,
      0, 0,			/* Position */
      DisplayWidth(display, screen), DisplayHeight(display, screen), /* Size */
      0,			/* Border width */
      CopyFromParent, CopyFromParent, CopyFromParent,
      CWBackPixel | CWOverrideRedirect,
      &xwa
    );
  }

  {
    Window root, *child_list = NULL;
    unsigned childs = 0;

    if(XQueryTree(display, mainwindow, &root, &parentwindow, &child_list, &childs)) {
      if(child_list) XFree(child_list);
    }
    else {
      parentwindow = rootwindow;
    }
    X_printf("X: X_init: parent window: 0x%x\n", (unsigned) parentwindow);
  }

  /*
   * used to communicate with kdos
   */
  if(kdos_client) {
    comm_atom = XInternAtom(display, "DOSEMU_COMM_ATOM", False);
    X_printf("X: X_init: got Atom: DOSEMU_COMM_ATOM = 0x%x\n", (unsigned) comm_atom);
  }

  X_printf("X: X_init: screen = %d, root = 0x%x, mainwindow = 0x%x\n",
    screen, (unsigned) rootwindow, (unsigned) mainwindow
  );

  load_cursor_shapes();

  if (font) {
    gcv.font = vga_font;
    normalgc = gc = XCreateGC(display, mainwindow, GCFont, &gcv);
    fullscreengc = XCreateGC(display, fullscreenwindow, GCFont, &gcv);
  } else {
    normalgc = gc = XCreateGC(display, mainwindow, 0, &gcv);
    fullscreengc = XCreateGC(display, fullscreenwindow, 0, &gcv);
  }

  attr.event_mask =
    KeyPressMask | KeyReleaseMask | KeymapStateMask |
    ButtonPressMask | ButtonReleaseMask |
    EnterWindowMask | LeaveWindowMask | PointerMotionMask |
    ExposureMask | StructureNotifyMask | FocusChangeMask |
    ColormapChangeMask;

  attr.cursor = X_standard_cursor;

  XChangeWindowAttributes(display, mainwindow, CWEventMask | CWCursor, &attr);
  XChangeWindowAttributes(display, fullscreenwindow, CWEventMask | CWCursor, &attr);

  XStoreName(display, mainwindow, config.X_title);
  XSetIconName(display, mainwindow, config.X_icon_name);
  xch.res_name  = "XDosEmu";
  xch.res_class = "XDosEmu";

  if (our_window) {
    XWMHints wmhint;
    wmhint.window_group = mainwindow;
    wmhint.input = True;
    wmhint.flags = WindowGroupHint | InputHint;
    XSetWMProperties(display, mainwindow, NULL, NULL, 
      dosemu_argv, dosemu_argc, NULL, &wmhint, &xch);
    XSetWMProperties(display, fullscreenwindow, NULL, NULL, 
      dosemu_argv, dosemu_argc, NULL, &wmhint, &xch);
  }
  else {
    XSetClassHint(display, mainwindow, &xch);
    XSetClassHint(display, fullscreenwindow, &xch);
  }
#if CONFIG_X_SELECTION
  /* Get atom for COMPOUND_TEXT/UTF8/TEXT type. */
  targets[TARGETS_ATOM] = XInternAtom(display, "TARGETS", False);
  targets[TIMESTAMP_ATOM] = XInternAtom(display, "TIMESTAMP", False);
  targets[COMPOUND_TARGET] = XInternAtom(display, "COMPOUND_TEXT", False);
  targets[UTF8_TARGET] = XInternAtom(display, "UTF8_STRING", False);
  targets[TEXT_TARGET] = XInternAtom(display, "TEXT", False);
  targets[STRING_TARGET] = XA_STRING;
#endif
  /* Delete-Window-Message black magic copied from xloadimage. */
  proto_atom  = XInternAtom(display, "WM_PROTOCOLS", False);
  delete_atom = XInternAtom(display, "WM_DELETE_WINDOW", False);
  if ((proto_atom != None) && (delete_atom != None)) {
    XChangeProperty(display, mainwindow,
      proto_atom, XA_ATOM, 32,
      PropModePrepend, (char *) &delete_atom, 1
    );
    XChangeProperty(display, fullscreenwindow,
      proto_atom, XA_ATOM, 32,
      PropModePrepend, (char *) &delete_atom, 1
    );
  }

  /* don't map window if set */
  if(getenv("DOSEMU_HIDE_WINDOW") == NULL) XMapWindow(display, mainwindow);

  if(have_true_color || have_shmap)
    Render_X.refresh_private_palette = NULL;
  register_render_system(&Render_X);
  register_text_system(&Text_X);

  /* initialize VGA emulator */
  if(vga_emu_init(remap_src_modes, &X_csd)) {
    error("X: X_init: VGAEmu init failed!\n");
    leavedos(99);
  }

  if(config.X_mgrab_key) grab_keystring = config.X_mgrab_key;
  if(*grab_keystring) grab_keysym = XStringToKeysym(grab_keystring);
  if(grab_keysym != NoSymbol) {
    X_printf("X: X_init: mouse grabbing enabled, use Ctrl+Mod1+%s to activate\n", grab_keystring);
  }
  else {
    X_printf("X: X_init: mouse grabbing disabled\n");
  }

  /* start with some standard text mode */
  X_set_videomode(TEXT, co, li);
  mouse_reset_to_current_video_mode();

  if (config.X_fullscreen)
    toggle_fullscreen_mode();

#if CONFIG_X_SPEAKER
  register_speaker(display, X_speaker_on, X_speaker_off);
#endif

  return 0;
}

/*
 * DANG_BEGIN_FUNCTION X_close
 *
 * description:
 * Destroy the window, unload font, pixmap and colormap.
 *
 * DANG_END_FUNCTION
 */
void X_close()
{
  X_printf("X: X_close\n");

  if(display == NULL) return;

#if CONFIG_X_SPEAKER
  /* turn off the sound, and */
  speaker_off();
  /* reset the speaker to it's default */
  register_speaker(NULL, NULL, NULL);
#endif

  if (kdos_client) kdos_close_msg();

#ifdef HAVE_XVIDMODE
  X_xf86vm_done();
#endif

  if (font) XUnloadFont(display, vga_font);
  if(our_window) {
    XDestroyWindow(display, normalwindow);
    XDestroyWindow(display, fullscreenwindow);
  }

  destroy_ximage();

  vga_emu_done();
  
  if(graphics_cmap) XFreeColormap(display, graphics_cmap);

  XFreeGC(display, gc);

  if(X_csd.pixel_lut != NULL) { free(X_csd.pixel_lut); X_csd.pixel_lut = NULL; }

  remapper_done();

#ifdef HAVE_DGA
  X_dga_done();
#endif

#ifdef HAVE_MITSHM
  X_shm_done();
#endif

  if(OldXErrorHandler != NULL) {
    XSetErrorHandler(OldXErrorHandler);
    OldXErrorHandler = NULL;
  }

  XCloseDisplay(display);
}


/*
 * Collect some information about the X server.
 */
void X_get_screen_info()
{
  XImage *xi;
  char *s;

  screen = DefaultScreen(display);
  rootwindow = RootWindow(display, screen);
  visual = DefaultVisual(display, DefaultScreen(display));

  have_true_color = visual->class == TrueColor || visual->class == DirectColor ? 1 : 0;

  switch(visual->class) {
    case StaticGray:  s = "StaticGray"; break;
    case GrayScale:   s = "GrayScale"; break;
    case StaticColor: s = "StaticColor"; break;
    case PseudoColor: s = "PseudoColor"; break;
    case TrueColor:   s = "TrueColor"; break;
    case DirectColor: s = "DirectColor"; break;
    default:          s = "Unknown";
  }
  X_printf("X: visual class is %s\n", s);

  if(have_true_color) {
    X_printf("X: using true color visual\n");
  }
  else {
    int i = DefaultDepth(display, DefaultScreen(display));

    if(i > 8) i = 8;				/* we will never try to get more */
    if((cmap_colors = 1 << i) == 1) {
      X_printf("X: Oops, your screen has only *one* color?\n");
    }
    else {
      X_printf("X: colormaps have %d colors\n", cmap_colors);
    }
  }

  xi = XCreateImage( /* this is just generated to allow measurements */
    display,
    visual,
    DefaultDepth(display, DefaultScreen(display)),
    ZPixmap, 0, NULL, 6 * 8, 2, 32, 0
  );

  if(xi != NULL) {
    if(xi->bytes_per_line % 6 == 0 && (xi->bytes_per_line / 6) == xi->bits_per_pixel) {
      ximage_bits_per_pixel = xi->bits_per_pixel;
      X_printf("X: pixel size is %d bits/pixel\n", ximage_bits_per_pixel);
    }
    else {
      ximage_bits_per_pixel = xi->bits_per_pixel;
      X_printf("X: could not determine pixel size, guessing %d bits/pixel\n", ximage_bits_per_pixel);
    }

    XDestroyImage(xi);
  }
  else {
    ximage_bits_per_pixel = DefaultDepth(display, DefaultScreen(display));
    X_printf("X: could not determine pixel size, guessing %d bits/pixel\n", ximage_bits_per_pixel);
  }

  X_csd.bits = ximage_bits_per_pixel;
  X_csd.bytes = (ximage_bits_per_pixel + 7) >> 3;
  X_csd.r_mask = visual->red_mask;
  X_csd.g_mask = visual->green_mask;
  X_csd.b_mask = visual->blue_mask;
  color_space_complete(&X_csd);
  if(X_csd.bits == 16 && (X_csd.r_bits + X_csd.g_bits + X_csd.b_bits) == 15) {
    X_csd.bits = ximage_bits_per_pixel = 15;
  }
}


#ifdef HAVE_MITSHM
/*
 * DANG_BEGIN_FUNCTION X_shm_init
 *
 * description:
 * Check availability of the MIT-SHM shared memory extension.
 *
 * DANG_END_FUNCTION
 */
void X_shm_init()
{
  int major_opcode, event_base, major_version, minor_version;
  Bool shared_pixmaps;

  shm_ok = 0;

  if(!config.X_mitshm) return;

  if(!XQueryExtension(display, "MIT-SHM", &major_opcode, &event_base, &shm_error_base)) {
    X_printf("X: X_shm_init: server does not support MIT-SHM\n");
    return;
  }

  X_printf("X: MIT-SHM ErrorBase: %d\n", shm_error_base);

  if(!XShmQueryVersion(display, &major_version, &minor_version, &shared_pixmaps)) {
    X_printf("X: X_shm_init: XShmQueryVersion() failed\n");
    return;
  }

  X_printf("X: using MIT-SHM\n");

  shm_ok = 1;
}


/*
 * DANG_BEGIN_FUNCTION X_shm_init
 *
 * description:
 * Turn off usage of the MIT-SHM shared memory extension.
 *
 * DANG_END_FUNCTION
 */
void X_shm_done()
{
  shm_ok = 0;
}
#endif


#ifdef HAVE_DGA
/*
 * Check for XFree86-DGA.
 */
void X_dga_init()
{
  int event_base, error_base, major_version, minor_version;

  dga_ok = 0;

  if(!XF86DGAQueryExtension(display, &event_base, &error_base)) {
    X_printf("X: server does not support XFree86-DGA\n");
    return;
  }

  X_printf("X: XFree86-DGA ErrorBase: %d\n", error_base);

  if(!XF86DGAQueryVersion(display, &major_version, &minor_version)) {
    X_printf("X: XF86DGAQueryVersion() failed\n");
    return;
  }

  X_printf("X: using XFree86-DGA\n");

  dga_ok = 1;
}


/*
 * Turn off XFree86-DGA.
 */
void X_dga_done()
{
  dga_ok = 0;
}
#endif

#ifdef HAVE_XVIDMODE
static void X_xf86vm_init(void)
{
  unsigned int eventB, errorB, ver, rev;
  if (XF86VidModeQueryExtension(display, &eventB, &errorB))
  {
    XF86VidModeQueryVersion(display, &ver, &rev);
    X_printf("X: VidMode Extension version %u.%u\n", ver, rev);
    XF86VidModeGetAllModeLines(display, 0, &modecount, &vidmode_modes);
    xf86vm_ok = 1;
  }
  else
  {
    X_printf("X: xvidmode: No VidMode Extension\n");
  }
}

static void X_xf86vm_done(void)
{
  if (mainwindow == fullscreenwindow)
    X_vidmode(-1, -1, &w_x_res, &w_y_res);
  xf86vm_ok = 0;
}

#endif 

/*
 * Handle 'auto'-entries in dosemu.conf, namely
 * $_X_keycode & $_layout
 *
 */
static void X_keymap_init()
{
  char *s = ServerVendor(display);

  if(s) X_printf("X: X_keymap_init: X server vendor is \"%s\"\n", s);
  if(config.X_keycode == 2 && s) {	/* auto */
#if defined(HAVE_UNICODE_KEYB) && defined(HAVE_XKB)
    /* All I need to know is that I'm using the X keyboard extension */
    config.X_keycode = using_xkb;
#else
    config.X_keycode = 0;

    /*
     * We could check some typical keycode/keysym translation; but
     * for now we just check the server vendor. XFree & XiGraphics
     * work. I don't know about Metro Link... -- sw
     */
    if(
      strstr(s, "The XFree86 Project") ||
      strstr(s, "Xi Graphics")
    ) config.X_keycode = 1;
#endif /* HAVE_UNICODE_KEYB && HAVE_XKB */
  }
  X_printf(
    "X: X_keymap_init: %susing DOSEMU's internal keycode translation\n",
    config.X_keycode ? "" : "we are not "
  );
#ifndef HAVE_UNICODE_KEYB
  if(config.X_keycode == 0) {
    X_printf("X: X_keymap_init: '$_layout'-entry will have no effect\n");
  }
#endif /* ! HAVE_UNICODE_KEYB */
}


/*
 * This function provides an interface to reconfigure parts
 * of X and the VGA emulation during a DOSEMU session.
 * It is used by the xmode.exe program that comes with DOSEMU.
 */
static int X_change_config(unsigned item, void *buf)
{
  int err = 0;
  XFontStruct *xfont;
  XGCValues gcv;

  X_printf("X: X_change_config: item = %d, buffer = 0x%x\n", item, (unsigned) buf);

  switch(item) {

    case X_CHG_TITLE:
       /* low-level write */
       if (buf)
       {
         X_printf("X: X_change_config: win_name = %s\n", (char *) buf);
         /* always change the normal window's title - never the full-screen one */
         XStoreName(display, normalwindow, buf);
       }
       /* high-level write (shows name of emulator + running app) */
       else
       {
         char title [X_TITLE_EMUNAME_MAXLEN + X_TITLE_APPNAME_MAXLEN + 35] = {0};
         
         /* app - DOS in a BOX */
         /* name of running application (if any) */
         if (config.X_title_show_appname && strlen (X_title_appname))
           strcpy (title, X_title_appname);
         
         /* append name of emulator */
         if (strlen (X_title_emuname))
         {
           if (strlen (title)) strcat (title, " - ");
           strcat (title, X_title_emuname);
         }
         else if (strlen (config.X_title))
         {
           if (strlen (title)) strcat (title, " - ");
           /* foreign string, cannot trust its length to be <= X_TITLE_EMUNAME_MAXLEN */
           snprintf (title + strlen (title), X_TITLE_EMUNAME_MAXLEN, "%s ", config.X_title);  
         }

         if (dosemu_frozen)
         {
           if (strlen (title)) strcat (title, " ");

           if (dosemu_user_froze)
             strcat (title, "[paused - Ctrl+Alt+P] ");
           else
             strcat (title, "[background pause] ");
         }

         if (grab_active || kbd_grab_active) {
	   strcat(title, "[");
	   if (kbd_grab_active) {
	     strcat(title, "keyboard");
	     if (grab_active)
	       strcat(title, "+");
	   }
	   if (grab_active)
	     strcat(title, "mouse");
	   strcat(title, " grab] ");
	 }

         /* now actually change the title of the Window */
         X_change_config (X_CHG_TITLE, title);
       }
       break;
       
    case X_CHG_TITLE_EMUNAME:
      X_printf ("X: X_change_config: emu_name = %s\n", (char *) buf);
      snprintf (X_title_emuname, X_TITLE_EMUNAME_MAXLEN, "%s", ( char *) buf);
      X_change_config (X_CHG_TITLE, NULL);
      break;
      
    case X_CHG_TITLE_APPNAME:
      X_printf ("X: X_change_config: app_name = %s\n", (char *) buf);
      snprintf (X_title_appname, X_TITLE_APPNAME_MAXLEN, "%s", (char *) buf);
      X_change_config (X_CHG_TITLE, NULL);
      break;

    case X_CHG_TITLE_SHOW_APPNAME:
      X_printf("X: X_change_config: show_appname %i\n", *((int *) buf));
      config.X_title_show_appname = *((int *) buf);
      X_change_config (X_CHG_TITLE, NULL);
      break;

    case X_CHG_FONT:
      xfont = XLoadQueryFont(display, (char *) buf);
      if(xfont == NULL) {
        if(font != NULL) XFreeFont(display, font);
        font = xfont;
        use_bitmap_font = TRUE;
	Text_X.Draw_string = bitmap_draw_string;
        X_printf("X: X_change_config: font \"%s\" not found, "
                 "using builtin\n", (char *) buf);
        X_printf("X: NOT loading a font. Using EGA/VGA builtin/RAM fonts.\n");
        X_printf("X: EGA/VGA font size is %d x %d\n",
              vga.char_width, vga.char_height);
        font_width = vga.char_width;
        font_height = vga.char_height;
        if(vga.mode_class == TEXT) X_resize_text_screen();
      }
      else {
        if(xfont->min_bounds.width != xfont->max_bounds.width) {
          X_printf("X: X_change_config: font \"%s\" is not monospaced\n", (char *) buf);
          XFreeFont(display, xfont);
        }
        else {
          if(font != NULL) XFreeFont(display, font);
          font = xfont;
	  use_bitmap_font = FALSE;
	  Text_X.Draw_string = X_draw_string;
          font_width  = font->max_bounds.width;
          font_height = font->max_bounds.ascent + font->max_bounds.descent;
          font_shift  = font->max_bounds.ascent;
          vga_font    = font->fid;
          gcv.font = vga_font;
          XChangeGC(display, gc, GCFont, &gcv);

          X_printf("X: X_change_config: using font \"%s\", size = %d x %d\n", (char *) buf, font_width, font_height);

          if(vga.mode_class == TEXT) X_resize_text_screen();
        }
      }
      break;

    case X_CHG_MAP:
      X_map_mode = *((int *) buf);
      X_printf("X: X_change_config: map window at mode 0x%02x\n", X_map_mode);
      if(X_map_mode == -1) { XMapWindow(display, mainwindow); }
      break;

    case X_CHG_UNMAP:
      X_unmap_mode = *((int *) buf);
      X_printf("X: X_change_config: unmap window at mode 0x%02x\n", X_unmap_mode);
      if(X_unmap_mode == -1) { XUnmapWindow(display, mainwindow); }
      break;

    case X_CHG_WINSIZE:
      config.X_winsize_x = *((int *) buf);
      config.X_winsize_y = ((int *) buf)[1];
      X_printf("X: X_change_config: set initial graphics window size to %d x %d\n", config.X_winsize_x, config.X_winsize_y);
      break;

    case X_CHG_BACKGROUND_PAUSE:
      X_printf("X: X_change_config: background_pause %i\n", *((int *) buf));
      config.X_background_pause = *((int *) buf);
      break;

    case X_CHG_FULLSCREEN:
      X_printf("X: X_change_config: fullscreen %i\n", *((int *) buf));
      if (*((int *) buf) == (mainwindow == normalwindow))
	toggle_fullscreen_mode();
      break;

    case X_GET_TITLE_APPNAME:
      snprintf (buf, X_TITLE_APPNAME_MAXLEN, "%s", X_title_appname);
      break;
          
    default:
      err = 100;
  }

  return err;
}


/*
 * The error handler just catches the first MIT-SHM errors.
 * This is needed to check for MIT-SHM.
 */
int NewXErrorHandler(Display *dsp, XErrorEvent *xev)
{
#ifdef HAVE_MITSHM
  if(xev->request_code == shm_error_base || xev->request_code == shm_error_base + 1) {
    X_printf("X::NewXErrorHandler: error using shared memory\n");
    shm_ok = 0;
  }
  else
#endif
  {
    return OldXErrorHandler(dsp, xev);
  }
#ifdef HAVE_MITSHM
  return 0;
#endif
}

/*
 * called exclusively by mouse INT33 funcs 1 and 2 of src/base/mouse/mouse.c
 * NOTE: We need this in addition to X_set_mouse_cursor to 'autoswitch'
 * on/off hiding the X cursor in graphics mode, when the DOS application
 * is drawing the cursor itself. We rely on the fact, that an application
 * wanting the mouse driver to draw the cursor will always use
 * INT33 functions 1 and 2, while others won't.
 *
 * So please, don't 'optimize' X_show_mouse_cursor away (;-), its
 * not bogus.		--Hans, 2000/06/01
 */
static void X_show_mouse_cursor(int yes)
{
   if (yes || vga.mode_class != GRAPH) {
      if (mouse_cursor_visible) return;
      if (grab_active) {
         XDefineCursor(display, normalwindow, X_mouse_nocursor);
         XDefineCursor(display, fullscreenwindow, X_mouse_nocursor);
      } else {
         XDefineCursor(display, normalwindow, X_standard_cursor);
         XDefineCursor(display, fullscreenwindow, X_standard_cursor);
      }
      mouse_cursor_visible = 1;
   } else {
      if (!mouse_cursor_visible) return;
      XDefineCursor(display, normalwindow, X_mouse_nocursor);
      XDefineCursor(display, fullscreenwindow, X_mouse_nocursor);
      mouse_cursor_visible = 0;
   }
}

static void toggle_kbd_grab(void)
{
  if(kbd_grab_active ^= 1) {
    X_printf("X: keyboard grab activated\n");
    if (mainwindow != fullscreenwindow) {
      XGrabKeyboard(display, mainwindow, True, GrabModeAsync, GrabModeAsync, CurrentTime);
    }
  }
  else {
    X_printf("X: keyboard grab released\n");
    if (mainwindow != fullscreenwindow) {
      XUngrabKeyboard(display, CurrentTime);
    }
  }
  X_change_config(X_CHG_TITLE, NULL);
}

static void toggle_mouse_grab(void)
{
  if(grab_active ^= 1) {
    config.mouse.use_absolute = 0;
    X_printf("X: mouse grab activated\n");
    if (mainwindow != fullscreenwindow) {
      XGrabPointer(display, mainwindow, True, PointerMotionMask | ButtonPressMask | ButtonReleaseMask,
                   GrabModeAsync, GrabModeAsync, mainwindow,  None, CurrentTime);
    }
    X_set_mouse_cursor(mouse_cursor_visible, mouse_x, mouse_y, w_x_res, w_y_res);
    mouse_enable_native_cursor(1);
  }
  else {
    config.mouse.use_absolute = 1;
    X_printf("X: mouse grab released\n");
    if (mainwindow != fullscreenwindow) {
      XUngrabPointer(display, CurrentTime);
    }
    X_set_mouse_cursor(mouse_cursor_visible, mouse_x, mouse_y, w_x_res, w_y_res);
    mouse_enable_native_cursor(0);
  }
  clear_selection_data();
  X_change_config(X_CHG_TITLE, NULL);
}

/*
 * DANG_BEGIN_FUNCTION X_set_mouse_cursor
 *
 * description:
 * called by mouse.c to hide/display the mouse and set it's position.
 * This is currently the only callback from mouse.c to X.
 *
 * DANG_END_FUNCTION
 *
 */  
static void X_set_mouse_cursor(int action, int mx, int my, int x_range, int y_range)
{
#if CONFIG_X_MOUSE
        int yes = action & 1;
	static Cursor *last_cursor = 0;
	Cursor *mouse_cursor_on, *mouse_cursor_off, *new_cursor;
	int x,y;

        if (action & 2)
          X_show_mouse_cursor(action >> 1);

        /* Figure out what cursor we want to show for visible/invisible */
	mouse_cursor_on = &X_standard_cursor;
	mouse_cursor_off = &X_mouse_nocursor;

	if (vga.mode_class != GRAPH) {
		/* So cut and past still works with a hidden cursor */
		mouse_cursor_off = &X_standard_cursor;
	}
	if (grab_active) {
		mouse_cursor_on = &X_mouse_nocursor;
		mouse_cursor_off = &X_mouse_nocursor;
	}
	/* See if we are visible/invisible */
	new_cursor = (yes)? mouse_cursor_on: mouse_cursor_off;

	/* Update the X cursor as needed */
	if (new_cursor != last_cursor) {
		XDefineCursor(display, fullscreenwindow, *new_cursor);
		XDefineCursor(display, normalwindow, *new_cursor);
		last_cursor = new_cursor;
	}

	/* Move the X cursor if needed */
	x = (x_range * mouse_x)/w_x_res;
	y = (y_range * mouse_y)/w_y_res;
	if ((mx != x) || (my != y)) {
		if (!grab_active && have_focus)
			XWarpPointer(display, None, mainwindow, 0, 0, 0, 0,
				     shift_x + (w_x_res * mx)/x_range,
				     shift_y + (w_y_res * my)/y_range);
	}
#endif /* CONFIG_X_MOUSE */
}

static void toggle_fullscreen_mode(void)
{
  unsigned resize_height, resize_width;

  XUnmapWindow(display, mainwindow);
  if (mainwindow == normalwindow) {
    X_printf("X: entering fullscreen mode\n");
    toggling_fullscreen = 2;
    saved_w_x_res = w_x_res;
    saved_w_y_res = w_y_res;
    if (!grab_active) {
      toggle_mouse_grab();
      force_grab = 1;
    }
    X_vidmode(x_res, y_res, &resize_width, &resize_height);
    mainwindow = fullscreenwindow;
    gc = fullscreengc;
    XCopyGC(display, normalgc, -1, gc);
    if (vga.mode_class == GRAPH || font == NULL)
      XResizeWindow(display, mainwindow, resize_width, resize_height);
    XMapWindow(display, mainwindow);
    XRaiseWindow(display, mainwindow);
    XGrabPointer(display, mainwindow, True,
                 PointerMotionMask | ButtonPressMask | ButtonReleaseMask,
                 GrabModeAsync, GrabModeAsync, mainwindow,  None,
                 CurrentTime);
    XGrabKeyboard(display, mainwindow, True, GrabModeAsync,
                  GrabModeAsync, CurrentTime);
  } else {
    X_printf("X: entering windowed mode!\n");
    toggling_fullscreen = 1;
    w_x_res = saved_w_x_res;
    w_y_res = saved_w_y_res;
    XUngrabKeyboard(display, CurrentTime);
    XUngrabPointer(display, CurrentTime);
    if (force_grab && grab_active) {
      toggle_mouse_grab();
    }
    force_grab = 0;
    mainwindow = normalwindow;
    X_vidmode(-1, -1, &resize_width, &resize_height);
    gc = normalgc;
    XCopyGC(display, fullscreengc, -1, gc);
    if (vga.mode_class == GRAPH || font == NULL)
      XResizeWindow(display, mainwindow, resize_width, resize_height);
    XMapWindow(display, mainwindow);
  }
  if(vga.mode_class == TEXT && font) {
    X_resize_text_screen();
  } else {	/* GRAPH or builtin font */
    resize_ximage(resize_width, resize_height);
    dirty_all_video_pages();
    X_update_screen();
  }
}

/*
 * DANG_BEGIN_FUNCTION X_handle_events
 *
 * description:
 * Handle pending X events (called from SIGALRM handler).
 *
 * DANG_END_FUNCTION
 */
static void X_handle_events(void)
{
   XEvent e, rel_evt;
   unsigned resize_width = w_x_res, resize_height = w_y_res, resize_event = 0;
   int keyrel_pending = 0;

#if CONFIG_X_MOUSE
   {
     static int lastingraphics = 0;
     if(vga.mode_class == GRAPH) {
       if(! lastingraphics) {
         lastingraphics = 1;
         X_show_mouse_cursor(0);
       }
     }
     else {
       if(lastingraphics) {
         lastingraphics = 0;
         X_show_mouse_cursor(1);
       }
     }
   }
#endif	/* CONFIG_X_MOUSE */

  while (XPending(display) > 0) 
    {
      XNextEvent(display,&e);

      switch(e.type) 
	{
       case Expose:
          /*
           * Well, if we're exposed we are most certainly mapped, too. :-)
           *
           * This is actually a kludge to work around some strange
           * effect related to the DOSEMU_WINDOW_ID stuff.
           *
           * The problem arises that, when the window is not created
           * by DOSEmu, it is typically already mapped. For some really
           * strange reason however, you cannot simply set is_mapped to TRUE.
           * Apparently there is some (black) magic going on somewhere in
           * DOSEmu that assumes that some initialisation should be
           * done while is_mapped is FALSE. I couldn't locate the
           * exact position, though.
           * 
           * -- 1998/08/09 sw
           */
          is_mapped = TRUE;

	  X_printf("X: expose event\n");
	  if(vga.mode_class == TEXT) {
	    if(e.xexpose.count == 0) X_redraw_text_screen();
	  }
	  else {	/* GRAPH */
	    if(!resize_event) put_ximage(
	      e.xexpose.x, e.xexpose.y,
	      e.xexpose.width, e.xexpose.height
	    );
	  }
	  break;

	case UnmapNotify:
	  X_printf("X: window unmapped\n");
	  is_mapped = FALSE;
	  break;
	  
	case MapNotify:
	  X_printf("X: window mapped\n");
	  is_mapped = TRUE;
	  break;
	  
	case FocusIn:
	  X_printf("X: focus in\n");
	  if (vga.mode_class == TEXT) text_gain_focus();
	  if (config.X_background_pause && !dosemu_user_froze) unfreeze_dosemu ();
	  break;

	case FocusOut:
	  X_printf("X: focus out\n");
	  if (mainwindow == fullscreenwindow) break;
	  if (vga.mode_class == TEXT) text_lose_focus();
	  output_byte_8042(port60_buffer | 0x80);
	  if (config.X_background_pause && !dosemu_user_froze) freeze_dosemu ();
	  break;

	case DestroyNotify:
	  X_printf("X: window got destroyed\n");
	  leavedos(99);
	  break;
	  
	case ClientMessage:
	  /* If we get a client message which has the value of the delete
	   * atom, it means the window manager wants us to die.
	   */
	  if(e.xclient.message_type == proto_atom && *e.xclient.data.l == delete_atom) {
	    X_printf("X: got window delete message\n");
	    /* XXX - Is it ok to call this from a SIGALRM handler? */
	    leavedos(0);
	    break;
	  }

	  if(e.xclient.message_type == comm_atom) {
	    kdos_recv_msg(e.xclient.data.b);
	  }
	  break;

    /* Selection-related events */
#if CONFIG_X_SELECTION
    /* The user made a selection in another window, so our selection
     * data is no longer needed. 
     */
	case SelectionClear:
	  clear_selection_data();
	  break;
	case SelectionNotify: 
	  scr_paste_primary(display,e.xselection.requestor,
			    e.xselection.property,True);
	  X_printf("X: SelectionNotify event\n");
	  break;
/* Some other window wants to paste our data, so send it. */
	case SelectionRequest:
	  send_selection(e.xselectionrequest.time,
			 e.xselectionrequest.requestor,
			 e.xselectionrequest.target,
			 e.xselectionrequest.property);
	  break;
#endif /* CONFIG_X_SELECTION */
		   
    /* Keyboard events */

	case KeyPress:
	  /* Autorepeat generates the unwanted release events, we filter
	   * them here. */
	  if (keyrel_pending && e.xkey.keycode == rel_evt.xkey.keycode &&
		e.xkey.time == rel_evt.xkey.time) {
	    X_printf("X_KBD: Ignoring fake release event, keycode=%#x\n",
		rel_evt.xkey.keycode);
	    keyrel_pending = 0;
	  }

          if((e.xkey.state & ControlMask) && (e.xkey.state & Mod1Mask)) {
            KeySym keysym = XKeycodeToKeysym(display, e.xkey.keycode, 0);
            if (keysym == grab_keysym) {
              force_grab = 0;
              toggle_mouse_grab();
              break;
            } else if (keysym == XK_k) {
              toggle_kbd_grab();
              break;
	    } else if (keysym == XK_p) {
              if (!dosemu_frozen) {
                freeze_dosemu_manual();
              } else {
                unfreeze_dosemu();
              }
              break;
            } else if (keysym == XK_f) {
              toggle_fullscreen_mode();
              break;
            }
          }
/* 
      Clears the visible selection if the cursor is inside the selection
*/
#if CONFIG_X_SELECTION
	  clear_if_in_selection();
#endif
	  X_process_key(&e.xkey);
	  break;

	case KeyRelease:
	  if (keyrel_pending) {
	    X_printf("X: duplicate KeyRelease event???\n");
	    X_process_key(&rel_evt.xkey);
	  }
	  rel_evt = e;
	  keyrel_pending = 1;
	  break;

	case KeymapNotify:
          X_printf("X: KeymapNotify event\n");
          /* don't process keys when doing fullscreen switching (this generates
             two events for fullscreen and one back to windowed mode )*/
          if (toggling_fullscreen) toggling_fullscreen--;
          X_process_keys(&e.xkeymap);
	  break;

    /* A keyboard mapping has been changed (e.g., with xmodmap). */
	case MappingNotify:  
	  X_printf("X: MappingNotify event\n");
	  XRefreshKeyboardMapping(&e.xmapping);
	  break;

/* Mouse events */
#if CONFIG_X_MOUSE  
	case ButtonPress:
#if 0 /* *** special debug code *** --sw */
        if(e.xbutton.button == Button1) {
          static unsigned flup = 0;
          v_printf("------: %u\n", ++flup);
        }
#endif /* *** end sdc *** */

#if CONFIG_X_SELECTION
	if (vga.mode_class == TEXT && !grab_active) {
	  if (e.xbutton.button == Button1)
	    start_selection(x_to_col(e.xbutton.x), y_to_row(e.xbutton.y));
	  else if (e.xbutton.button == Button3)
	    start_extend_selection(x_to_col(e.xbutton.x), y_to_row(e.xbutton.y));
	}
#endif /* CONFIG_X_SELECTION */
	  set_mouse_position(e.xmotion.x,e.xmotion.y); /*root@sjoerd*/
	  set_mouse_buttons(e.xbutton.state|(0x80<<e.xbutton.button));
	  break;
	  
	case ButtonRelease:
	  set_mouse_position(e.xmotion.x,e.xmotion.y);  /*root@sjoerd*/
#if CONFIG_X_SELECTION
	  if (vga.mode_class == TEXT) switch (e.xbutton.button)
	    {
	    case Button1 :
	    case Button3 : 
	      sel_text = end_selection();
	      sel_time = e.xbutton.time;
	      if (sel_text == NULL)
		break;
	      XSetSelectionOwner(display, XA_PRIMARY, mainwindow, sel_time);
	      if (XGetSelectionOwner(display, XA_PRIMARY) != mainwindow)
		{
		  X_printf("X: Couldn't get primary selection!\n");
		  return;
		}
	      XChangeProperty(display, rootwindow, XA_CUT_BUFFER0, XA_STRING,
			      8, PropModeReplace, sel_text, strlen(sel_text));

	      break;
	    case Button2 :
	      X_printf("X: mouse Button2Release\n");
	      scr_request_selection(display,mainwindow,e.xbutton.time,
				    x_to_col(e.xbutton.x),
				    y_to_row(e.xbutton.y));
	      break;
	    }
#endif /* CONFIG_X_SELECTION */
	  set_mouse_buttons(e.xbutton.state&~(0x80<<e.xbutton.button));
	  break;
	  
	case MotionNotify:
#if CONFIG_X_SELECTION
	  extend_selection(x_to_col(e.xmotion.x), y_to_row(e.xmotion.y));
#endif /* CONFIG_X_SELECTION */
	  set_mouse_position(e.xmotion.x, e.xmotion.y);
	  break;
	  
	case EnterNotify:
	  /* Here we trigger the kludge to snap in the cursor
	   * drawn by win31 and align it with the X-cursor.
	   * (Hans)
	   */
          X_printf("X: Mouse entering window event\n");
          if (mouse_really_left_window)
          {
            X_printf("X: Mouse really entering window\n");
	    if(!grab_active) snap_X=3;
	    set_mouse_position(e.xcrossing.x, e.xcrossing.y);
	    set_mouse_buttons(e.xcrossing.state);
          }
	  break;
	  
	case LeaveNotify:                   /* Should this do anything? */
	  X_printf("X: Mouse leaving window, coordinates %d %d\n", e.xcrossing.x, e.xcrossing.y);
          /* some stupid window managers send this event if you click a
             mouse button when the mouse is in the window. Let's ignore the
             next "EnterNotify" if the mouse doesn't really leave the window */
          mouse_really_left_window = 1;
          if (e.xcrossing.x >= 0 && e.xcrossing.x < w_x_res &&
              e.xcrossing.y >= 0 && e.xcrossing.y < w_y_res) {
            X_printf("X: bogus LeaveNotify event\n");
            mouse_really_left_window = 0;
          }
	  break;

        case ConfigureNotify:
          /* printf("X: configure event: width = %d, height = %d\n", e.xconfigure.width, e.xconfigure.height); */
 	/* DANG_BEGIN_REMARK
 	 * DO NOT REMOVE THIS TEST!!!
 	 * It is magic, without it EMS fails on my machine under X.
 	 * Perhaps someday when we don't use a buggy /proc/self/mem..
 	 * -- EB 18 May 1998
	 * A slightly further look says it's not the test so much as
	 * suppressing noop resize events...
	 * -- EB 1 June 1998
 	 * DANG_END_REMARK
 	 */
          if ((e.xconfigure.width != resize_width)
                       || (e.xconfigure.height != resize_height)) {
            resize_event = 1;
            resize_width = e.xconfigure.width;
            resize_height = e.xconfigure.height;
          }
          break;

#endif /* CONFIG_X_MOUSE */
/* some weirder things... */
	}
    }

    if (keyrel_pending) {
#if CONFIG_X_SELECTION
      clear_if_in_selection();
#endif
      X_process_key(&rel_evt.xkey);
      keyrel_pending = 0;
    }

    if(ximage != NULL && resize_event) {
      if(ximage->width == resize_width && ximage->height == resize_height) resize_event = 0;
    }

    if(resize_event && mainwindow == normalwindow) {
      resize_ximage(resize_width, resize_height);
      dirty_all_video_pages();
      if (vga.mode_class == TEXT)
	vga.reconfig.mem = 1;
      X_update_screen();
    }

#if CONFIG_X_MOUSE  
  do_mouse_irq();
#endif  
}


/*
 * DANG_BEGIN_FUNCTION graphics_cmap_init
 *
 * description:
 * Allocate a colormap for graphics modes and initialize it.
 * Do mostly nothing on true color displays.
 * Otherwise, do:
 *   - if colormaps have less than 256 entries (notably 16 or 2 colors),
 *     don't use a private colormap
 *   - if a shared map is requested and there are less than 36 colors (3/4/3)
 *     available, use a private colormap
 *
 * Note: Text modes always use the screen's default colormap.
 *
 * DANG_END_FUNCTION
 *
 * Note: The above is not fully true. If on 16 or 2 color displays the
 * allocation of a shared map fails for some reason, the code still tries
 * to get a private map (and will likely fail). -- sw
 */
void graphics_cmap_init()
{
  /* Note: should really deallocate and reallocate our colours every time
   we switch graphic modes... --adm */

  if(have_true_color) return;

  if(graphics_cmap == 0) {	/* only the first time! */
    have_shmap = FALSE;

    if(config.X_sharecmap || cmap_colors < 256) {		/* notably on 16 color and mono screens */
      graphics_cmap = DefaultColormap(display, screen);
      X_csd = MakeSharedColormap();

      if(X_csd.bits == 1) {
        X_printf("X: graphics_cmap_init: couldn't get enough free colors; trying private colormap\n");
        have_shmap = FALSE;
      }
      else {
        if(X_csd.bits < 4 * 5 * 4) {
          X_printf("X: graphics_cmap_init: couldn't get many free colors (%d). May look bad.\n", X_csd.bits);
        }
      }

      if(X_csd.bits != 1) have_shmap = TRUE;
    }

    if(!have_shmap) {
      if(MakePrivateColormap()) {
        X_printf("X: graphics_cmap_init: using private colormap.\n");
      }
      else {
        graphics_cmap = 0;
        error("X: graphics_cmap_init: Couldn't get a colormap for graphics modes!\n");
      }
    }
    else {
      X_printf("X: graphics_cmap_init: using shared colormap.\n");
    }
  }
  else {
    X_printf("X: graphics_cmap_init: unexpectedly called\n");
  }
}


/*
 * Create a private colormap. The colormap is *not* initialized.
 */
int MakePrivateColormap()
{
  int i;
  unsigned long pixels[256];		/* ok, cmap_colors always <= 256 */

  graphics_cmap = XCreateColormap(display, rootwindow, visual, AllocNone);

  i = XAllocColorCells(display, graphics_cmap, True, NULL, 0, pixels, cmap_colors);
  if(!i) {
    X_printf("X: failed to allocate private color map (no PseudoColor visual)\n");
    return 0;
  }

  return 1;
}


/*
 * Create a shared colormap. The colormap's features are
 * described with a ColorSpaceDesc object (cf. graphics_cmap_init).
 */
ColorSpaceDesc MakeSharedColormap()
{
  ColorSpaceDesc csd;
  XColor xc;
  int i, j;
  static unsigned long pix[256];
  static c_cube c_cubes[] = {
    { 6, 8, 5 }, { 6, 7, 5 }, { 6, 6, 5 }, { 5, 7, 5 },
    { 5, 6, 5 }, { 4, 8, 4 }, { 5, 6, 4 }, { 5, 5, 4 },
    { 4, 5, 4 }, { 4, 5, 3 }, { 4, 4, 3 }, { 3, 4, 3 }
  };

  xc.flags = DoRed | DoGreen | DoBlue;

  csd.bytes = 1;
  csd.pixel_lut = NULL;
  csd.r_mask = csd.g_mask = csd.b_mask = 0;

  for(i = j = 0; i < sizeof(c_cubes) / sizeof(*c_cubes); i++) {
    if((j = try_cube(pix, c_cubes + i))) break;
  }

  if(!j) {
    X_printf("X: MakeSharedColormap: failed to allocate shared color map\n");
    csd.r_bits = 1;
    csd.g_bits = 1;
    csd.b_bits = 1;
  }
  else {
    csd.r_bits = c_cubes[i].r;
    csd.g_bits = c_cubes[i].g;
    csd.b_bits = c_cubes[i].b;
  }

  csd.r_shift = 1;
  csd.g_shift = csd.r_bits * csd.r_shift;
  csd.b_shift = csd.g_bits * csd.g_shift;

  csd.bits = csd.r_bits * csd.g_bits * csd.b_bits;

  if(csd.bits > 1) {
    csd.pixel_lut = malloc(csd.bits * sizeof(*csd.pixel_lut));
    for(i = 0; i < csd.bits; i++) csd.pixel_lut[i] = pix[i];
  }

  X_printf("X: MakeSharedColormap: RGBCube %d - %d - %d (%d colors)\n", csd.r_bits, csd.g_bits, csd.b_bits, csd.bits);

  return csd;
}


/*
 * Try to allocate a color cube (cf. MakeSharedColormap).
 */
int try_cube(unsigned long *p, c_cube *c)
{
  unsigned r, g, b;
  int i = 0;
  XColor xc;

  xc.flags = DoRed | DoGreen | DoBlue;

  for(b = 0; b < c->b; b++) for(g = 0; g < c->g; g++) for(r = 0; r < c->r; r++) {
    xc.red = (0xffff * r) / c->r;
    xc.green = (0xffff * g) / c->g;
    xc.blue = (0xffff * b) / c->b;
    if(XAllocColor(display, graphics_cmap, &xc)) {
      p[i++] = xc.pixel;
    }
    else {
      while(--i >= 0) XFreeColors(display, graphics_cmap, p + i, 1, 0);
      return 0;
    }
  }
  return i;
}


/*
 * Update the active X colormap for text modes DAC entry col.
 */
void X_set_text_palette(DAC_entry col)
{
  int read_cmap = 1;
  int i, shift = 16 - dac_bits;
  XColor xc;

  xc.flags = DoRed | DoGreen | DoBlue;
  xc.pixel = text_colors[i = col.index];
  xc.red   = col.r << shift;
  xc.green = col.g << shift;
  xc.blue  = col.b << shift;

  if(text_col_stats[i]) XFreeColors(display, text_cmap, &xc.pixel, 1, 0);

  if(!(text_col_stats[i] = XAllocColor(display, text_cmap, &xc))) {
    get_approx_color(&xc, text_cmap, read_cmap);
    read_cmap = 0;
    X_printf("X: refresh_text_palette: %d (%d -> app. %d)\n", i, (int) text_colors[i], (int) xc.pixel);
      
  }
  else {
    X_printf("X: refresh_text_palette: %d (%d -> %d)\n", i, (int) text_colors[i], (int) xc.pixel);
  }
  text_colors[i] = xc.pixel;
}


/*
 * Update the private palette to match the current DAC entries.
 */
void refresh_private_palette()
{
  DAC_entry col[256];
  XColor xcolor[256];
  RGBColor c;
  int i, j, k;
  unsigned bits, shift;

  /*
   * Colormap size is limited to cmap_colors, not 256. This leads to
   * incorrect displays at screens with less than 8 bit color depth.
   * --> So don't use private palettes with less than 256 colors.
   */

  j = changed_vga_colors(col);

  for(i = k = 0; k < j; k++) {
    if(col[k].index < cmap_colors) {
      c.r = col[k].r; c.g = col[k].g; c.b = col[k].b;
      bits = dac_bits;
      gamma_correct(&remap_obj, &c, &bits);
      shift = 16 - bits;
      xcolor[i].flags = DoRed | DoGreen | DoBlue;
      xcolor[i].pixel = col[k].index;
      xcolor[i].red   = c.r << shift;
      xcolor[i].green = c.g << shift;
      xcolor[i].blue  = c.b << shift;
#if 0
      X_printf(
        "X: refresh_private_palette: color 0x%02x (0x%02x 0x%02x 0x%02x) -> (0x%04x 0x%04x 0x%04x)\n",
        (unsigned) col[k].index, (unsigned) c.r, (unsigned) c.g, (unsigned) c.b,
        (unsigned) xcolor[i].red, (unsigned) xcolor[i].green, (unsigned) xcolor[i].blue
      );
#else
      X_printf("X: refresh_private_palette: color 0x%02x\n", (unsigned) col[k].index);
#endif
      i++;
    }
    else {
      X_printf("X: refresh_private_palette: color 0x%02x not updated\n", (unsigned) col[k].index);
    }
  }

  if(graphics_cmap && i) XStoreColors(display, graphics_cmap, xcolor, i);
}


/*
 * Scans the colormap cmap for the color closest to xc and
 * returns it in xc. The color values used by cmap are queried
 * from the X server only if read_cmap is set.
 */
void get_approx_color(XColor *xc, Colormap cmap, int read_cmap)
{
  int i, d0, ind;
  unsigned d1, d2;
  static XColor xcols[256];

  if(read_cmap) {
    for(i = 0; i < cmap_colors; i++) xcols[i].pixel = i;
    XQueryColors(display, cmap, xcols, cmap_colors);
  }

  ind = -1; d2 = -1;
  for(i = 0; i < cmap_colors; i++) {
    /* Note: X color values are unsigned short, so using int for the sum (d1) should be ok. */
    d0 = (int) xc->red - xcols[i].red; d1 = abs(d0);
    d0 = (int) xc->green - xcols[i].green; d1 += abs(d0);
    d0 = (int) xc->blue - xcols[i].blue; d1 += abs(d0);
    if(d1 < d2) ind = i, d2 = d1;
  }

  if(ind >= 0) *xc = xcols[ind];
}


/*
 * Create an image. Will use shared memory, if available.
 * The image is used in graphics modes only.
 */
void create_ximage()
{
#ifdef HAVE_MITSHM
  if(shm_ok) {
    ximage = XShmCreateImage(
      display,
      visual,
      DefaultDepth(display,DefaultScreen(display)),
      ZPixmap, NULL,
      &shminfo,
      w_x_res, w_y_res
    );

    if(ximage == NULL) {
      X_printf("X: XShmCreateImage() failed\n");
      shm_ok = 0;
    }
    else {
      shminfo.shmid = shmget(IPC_PRIVATE, ximage->bytes_per_line * w_y_res, IPC_CREAT | 0777);
      if(shminfo.shmid < 0) {
        X_printf("X: shmget() failed\n");
        XDestroyImage(ximage);
        ximage = NULL;
        shm_ok = 0;
      }
      else {
        shminfo.shmaddr = (char *) shmat(shminfo.shmid, 0, 0);
        if(shminfo.shmaddr == ((char *) -1)) {
          X_printf("X: shmat() failed\n");
          XDestroyImage(ximage);
          ximage = NULL;
          shm_ok = 0;
        }
        else {
          shminfo.readOnly = False;
          XShmAttach(display, &shminfo);
          shmctl(shminfo.shmid, IPC_RMID, 0 );
          ximage->data = shminfo.shmaddr;
          XSync(display, False);	/* MIT-SHM will typically fail here... */
        }
      }
    }
  }
  if(!shm_ok)
#endif
  {
    ximage = XCreateImage(
      display,
      visual,
      DefaultDepth(display,DefaultScreen(display)),
      ZPixmap, 0, NULL,
      w_x_res, w_y_res,
      32, 0
    );
    if(ximage != NULL) {
      ximage->data = malloc(ximage->bytes_per_line * w_y_res);
      if(ximage->data == NULL) {
        X_printf("X: failed to allocate memory for XImage of size %d x %d\n", w_x_res, w_y_res);
      }
    }
    else {
      X_printf("X: failed to create XImage of size %d x %d\n", w_x_res, w_y_res);
    }
  }
  XSync(display, False);
}


/*
 * Destroy an image.
 */
void destroy_ximage()
{
  if(ximage == NULL) return;

#ifdef HAVE_MITSHM
  if(shm_ok) XShmDetach(display, &shminfo);
#endif

  XDestroyImage(ximage);

#ifdef HAVE_MITSHM
  if(shm_ok) shmdt(shminfo.shmaddr);
#endif

  ximage = NULL;
}


/*
 * Display part of an image.
 */
void put_ximage(int x, int y, unsigned width, unsigned height)
{
#ifdef HAVE_MITSHM
  if(shm_ok)
    XShmPutImage(display, mainwindow, gc, ximage, x, y, x, y, width, height, True);
  else
#endif /* HAVE_MITSHM */
    XPutImage(display, mainwindow, gc, ximage, x, y, x, y, width, height);
}


/*
 * Resize an image.
 */
void resize_ximage(unsigned width, unsigned height)
{
  X_printf("X: resize_ximage %d x %d --> %d x %d\n", w_x_res, w_y_res, width, height);
  destroy_ximage();
  w_x_res = width;
  w_y_res = height;
  create_ximage();
  remap_obj.dst_resize(&remap_obj, width, height, ximage->bytes_per_line);
  remap_obj.dst_image = ximage->data;
}

/*
 * Resize the window to given (*) size and lock it at that size
 * In text mode, you have to resize the mapper, too
 * ((*): given size is size of the embedded image)
 */
static void lock_window_size(unsigned wx_res, unsigned wy_res)
{
  XSizeHints sh;
  int x_fill, y_fill;

  sh.width = sh.min_width = sh.max_width = wx_res;
  sh.height = sh.min_height = sh.max_height = wy_res;

  sh.flags = PSize  | PMinSize | PMaxSize;
  if(config.X_fixed_aspect || config.X_aspect_43) sh.flags |= PAspect;
  if (font == NULL) {
    sh.flags |= PResizeInc;
    sh.max_width = 32767;
    sh.max_height = 32767;
    sh.min_width = 0;
    sh.min_height = 0;
    sh.width_inc = 1;
    sh.height_inc = 1;
  }
  sh.min_aspect.x = w_x_res;
  sh.min_aspect.y = w_y_res;
  sh.max_aspect = sh.min_aspect;
  XSetNormalHints(display, normalwindow, &sh);
  XSync(display, False);

  x_fill = w_x_res;
  y_fill = w_y_res;
  if (mainwindow == fullscreenwindow)
    X_vidmode(x_res, y_res, &x_fill, &y_fill);

  XResizeWindow(display, mainwindow, x_fill, y_fill);
  X_printf("Resizing our window to %dx%d image\n", x_fill, y_fill);

  XSetForeground(display, gc, text_colors[0]);
  XFillRectangle(display, mainwindow, gc, 0, 0, x_fill, y_fill);

  XSync(display, False); /* show NOW */
  if (font == NULL) {
    resize_text_mapper(ximage_mode);
    resize_ximage(x_fill, y_fill);    /* destroy, create, dst-map */
    *remap_obj.dst_color_space = X_csd;
  }
}

/* 
 * DANG_BEGIN_FUNCTION X_set_videomode
 *
 * description:
 * This is the interface function called by the video subsystem
 * to set a video mode.
 *
 * NOTE: The actual mode is taken from the global variable "video_mode".
 *
 * description:
 * Set the video mode.
 * If mode_class is -1, this will only reinitialize the current mode.
 * The other arguments are ignored in this case.
 *
 * DANG_END_FUNCTION
 */
int X_set_videomode(int mode_class, int text_width, int text_height)
{
  int mode = video_mode;
  XSizeHints sh; /* for graphics modes, text size locking is above */
#ifdef X_USE_BACKING_STORE
  XSetWindowAttributes xwa;
#endif

  if(mode_class != -1) {	/* tell vgaemu we're going to another mode */
    if(!vga_emu_setmode(mode, text_width, text_height)) {
      v_printf("vga_emu_setmode(%d, %d, %d) failed\n",
               mode, text_width, text_height);
      return 0;
    } else if (font == NULL) {
      font_width = vga.char_width;
      font_height = vga.char_height;
    }
  }
                               
  X_printf("X: X_setmode: %svideo_mode 0x%x (%s), size %d x %d (%d x %d pixel)\n",
    mode_class != -1 ? "" : "re-init ",
    (int) mode, vga.mode_class ? "GRAPH" : "TEXT",
    vga.text_width, vga.text_height, vga.width, vga.height
  );

  if(X_unmap_mode != -1 && (X_unmap_mode == vga.mode || X_unmap_mode == vga.VESA_mode)) {
    XUnmapWindow(display, mainwindow);
    X_unmap_mode = -1;
  }

  destroy_ximage();

  mouse_x = mouse_y = 0;

#ifdef X_USE_BACKING_STORE
  /*
   * We use it only in text modes; in graphics modes we are fast enough and
   * it would likely only slow down the whole thing. -- sw
   */
  if(vga.mode_class == TEXT && font) {
    xwa.backing_store = Always;
    xwa.backing_planes = -1;
    xwa.save_under = True;
  }
  else {	/* GRAPH */
    xwa.backing_store = NotUseful;
    xwa.backing_planes = 0;
    xwa.save_under = False;
  }

  XChangeWindowAttributes(display, mainwindow, CWBackingStore | CWBackingPlanes | CWSaveUnder, &xwa);
#endif

  if(vga.mode_class == TEXT) {
    XSetWindowColormap(display, mainwindow, text_cmap);

    X_reset_redraw_text_screen();

    dac_bits = vga.dac.bits;

    if (font) {
      w_x_res = x_res = vga.text_width * font_width;
      w_y_res = y_res = vga.text_height * font_height;
    } else {
      x_res = vga.width;
      w_x_res = (x_res <= 320) ? (2 * x_res) : x_res;
      y_res = vga.height;
      w_y_res = (y_res <= 240) ? (2 * y_res) : y_res;
    }

    saved_w_x_res = w_x_res;
    saved_w_y_res = w_y_res;
    lock_window_size(w_x_res, w_y_res);
    if(mainwindow == fullscreenwindow) {
      X_vidmode(x_res, y_res, &w_x_res, &w_y_res);
    }
  }
  else {	/* GRAPH */

    if(!have_true_color) {
      XSetWindowColormap(display, mainwindow, graphics_cmap);
    }

    dac_bits = vga.dac.bits;

    x_res = vga.width;
    y_res = vga.height;

    get_mode_parameters(&w_x_res, &w_y_res, ximage_mode, &veut);
    if(mainwindow == fullscreenwindow) {
      saved_w_x_res = w_x_res;
      saved_w_y_res = w_y_res;
      X_vidmode(x_res, y_res, &w_x_res, &w_y_res);
    }

    create_ximage();

    remap_obj.dst_image = ximage->data;
    *remap_obj.dst_color_space = X_csd;
    remap_obj.dst_resize(&remap_obj, w_x_res, w_y_res, ximage->bytes_per_line);

    sh.width = w_x_res;
    sh.height = w_y_res;
    if(!(remap_obj.state & ROS_SCALE_ALL)) {
      sh.width_inc = x_res;
      sh.height_inc = y_res;
    }
    else {
      sh.width_inc = 1;
      sh.height_inc = 1;
    }
    sh.min_aspect.x = w_x_res;
    sh.min_aspect.y = w_y_res;
    sh.max_aspect = sh.min_aspect;

    if(remap_obj.state & ROS_SCALE_ALL) {
      sh.min_width = 0;
      sh.min_height = 0;
    }
    else {
      sh.min_width = w_x_res;
      sh.min_height = w_y_res;
    }
    if(remap_obj.state & ROS_SCALE_ALL) {
      sh.max_width = 32767;
      sh.max_height = 32767;
    }
    else if(remap_obj.state & ROS_SCALE_2) {
      sh.max_width = x_res << 1;
      sh.max_height = y_res << 1;
    }
    else {
      sh.max_width = w_x_res;
      sh.max_height = w_y_res;
    }

    sh.flags = PResizeInc | PSize  | PMinSize | PMaxSize;
    if(config.X_fixed_aspect || config.X_aspect_43) sh.flags |= PAspect;

    XSetNormalHints(display, normalwindow, &sh);
    XResizeWindow(display, mainwindow, w_x_res, w_y_res);
  }

  /* unconditionally update the palette */

  if(X_map_mode != -1 && (X_map_mode == vga.mode || X_map_mode == vga.VESA_mode)) {
    XMapWindow(display, mainwindow);
    X_map_mode = -1;
  }

  return 1;
}

/*
 * Resize the X display to the appropriate size.
 */
void X_resize_text_screen() 
{
  if (font) {
    w_x_res = x_res = vga.text_width * font_width;
    w_y_res = y_res = vga.text_height * font_height;
  } else {
    font_width = vga.char_width;
    font_height = vga.char_height;
    x_res = vga.width;
    w_x_res = (x_res <= 320) ? (2 * x_res) : x_res;
    y_res = vga.height;
    w_y_res = (y_res <= 240) ? (2 * y_res) : y_res;
  }
  saved_w_x_res = w_x_res;
  saved_w_y_res = w_y_res;
  
  lock_window_size(w_x_res, w_y_res);
 
  X_redraw_text_screen();
}

/*
 * Change to requested video mode or the closest greater one.
 */
static void X_vidmode(int w, int h, int *new_width, int *new_height)
{
  int nw, nh, dw, dh, mx, my;
  
  nw = dw = DisplayWidth(display, screen);
  nh = dh = DisplayHeight(display, screen);
  
#ifdef HAVE_XVIDMODE
  if (xf86vm_ok) {
    static XF86VidModeModeLine vidmode_modeline;
    static int viewport_x, viewport_y, dotclock;
    int vx = 0, vy = 0;
    int i, j, restore_dotclock = 0;

    if (w == -1 && h == -1) { /* need to perform reset to windowed mode */
      w = vidmode_modeline.hdisplay;
      h = vidmode_modeline.vdisplay;
      vx = viewport_x;
      vy = viewport_y;
      restore_dotclock = 1;
    } else if (mainwindow != fullscreenwindow) {
      XF86VidModeGetModeLine(display,screen,&dotclock,&vidmode_modeline);
      XF86VidModeGetViewPort(display,screen,&viewport_x,&viewport_y);
      mainwindow = fullscreenwindow;
    }
    j = -1;
    for (i=0; i<modecount; i++) {
      if ((vidmode_modes[i]->hdisplay >= w) && 
          (vidmode_modes[i]->vdisplay >= h) &&
          (vidmode_modes[i]->hdisplay <= nw) &&
          (vidmode_modes[i]->vdisplay <= nh) &&
	  (!restore_dotclock || vidmode_modes[i]->dotclock == dotclock) &&
	  ( j == -1 ||
	    vidmode_modes[i]->dotclock >= vidmode_modes[j]->dotclock ||
	    vidmode_modes[i]->hdisplay != nw ||
	    vidmode_modes[i]->vdisplay != nh)) {
        nw = vidmode_modes[i]->hdisplay;
        nh = vidmode_modes[i]->vdisplay;
        j = i;
      }
    }
    if (j == -1) {
      error("X: vidmode for (%d,%d) not found!\n", w, h);
      *new_width = w;
      *new_height = h;
      return;
    }

    X_printf("X: vidmode asking for (%d,%d); setting (%d,%d)\n", w, h, nw, nh);
    XF86VidModeSwitchToMode(display,screen,vidmode_modes[j]);
    XF86VidModeSetViewPort (display,screen,vx,vy);
  }
#endif

  if (mainwindow == normalwindow) {
    nw = w_x_res;
    nh = w_y_res;
  }
          
  mx = MIN(mouse_x, nw - 1);
  my = MIN(mouse_y, nh - 1);
  shift_x = 0;
  shift_y = 0;
  if(vga.mode_class == TEXT && font) {
    shift_x = (nw - w_x_res)/2;
    shift_y = (nh - w_y_res)/2;
  }

  if (!grab_active && (mx != 0 || my != 0) && have_focus)
    XWarpPointer(display, None, mainwindow, 0, 0, 0, 0,
                 mx + shift_x, my + shift_y);
  *new_width = nw;
  *new_height = nh;
}

/*
 * Sync prev_screen & screen_adr.
 */
void X_reset_redraw_text_screen()
{
  if(!is_mapped) return;
  reset_redraw_text_screen();
}

void X_redraw_text_screen()
{
  if(!is_mapped) return;
  redraw_text_screen();
}

int X_update_screen()
{
  return update_screen(&veut, is_mapped);
}

/* 
 * Change color values in our graphics context 'gc'.
 */
void set_gc_attr(Bit8u attr)
{
  XGCValues gcv;

  gcv.foreground = text_colors[ATTR_FG(attr)];
  gcv.background = text_colors[ATTR_BG(attr)];
  XChangeGC(display, gc, GCForeground | GCBackground, &gcv);
}


/*
 * Draw a non-bitmap text string.
 * The attribute is the VGA color/mono text attribute.
 */
static void X_draw_string(int x, int y, unsigned char *text, int len, Bit8u attr)
{
  set_gc_attr(attr);
  XDrawImageString(
    display, mainwindow, gc,
    shift_x + font_width * x,
    shift_y + font_height * y + font_shift,
    text,
    len
    );
}

/*
 * Draw a text string for bitmap fonts.
 * The attribute is the VGA color/mono text attribute.
 */
static void bitmap_draw_string(int x, int y, unsigned char *text, int len, Bit8u attr)
{
  RectArea ra = convert_bitmap_string(x, y, text, len, attr);
  /* put_ximage uses display, mainwindow, gc, ximage       */
  X_printf("image at %d %d %d %d\n", ra.x, ra.y, ra.width, ra.height);
  if (ra.width)
    put_ximage(ra.x, ra.y, ra.width, ra.height);
}

/*
 * Draw a horizontal line (for text modes)
 * The attribute is the VGA color/mono text attribute.
 */
void X_draw_line(int x, int y, int len)
{
  XDrawLine(
      display, mainwindow, gc,
      (shift_x + font_width * x) * w_x_res / x_res,
      (font_height * y + font_shift) * w_y_res / y_res,
      (shift_x + font_width * (x + len) - 1) * w_x_res / x_res,
      (shift_y + font_height * y + font_shift) * w_y_res / y_res
    );
}

/*
 * Load the main text font. Try first the user specified font, then
 * vga, then 9x15 and finally fixed. If none of these exists and
 * is monospaced, give up (shouldn't happen).
 */
void load_text_font()
{
  const char *p = config.X_font;
  font = NULL;
  use_bitmap_font = TRUE;
  Text_X.Draw_string = bitmap_draw_string;
  if (p && strlen(p)) {
    font = XLoadQueryFont(display, p);
    if(font == NULL) {
      error("X: Unable to open font \"%s\", using builtin\n", p);
    }
    else if(font->min_bounds.width != font->max_bounds.width) {
      error("X: Font \"%s\" isn't monospaced, using builtin\n", p);
      XFreeFont(display, font);
      font = NULL;
    }
    else {
      font_width = font->max_bounds.width;
      font_height = font->max_bounds.ascent + font->max_bounds.descent;
      font_shift = font->max_bounds.ascent;
      vga_font = font->fid;
      X_printf("X: Using font \"%s\", size = %d x %d\n", p, font_width, font_height);
      use_bitmap_font = FALSE;
      Text_X.Draw_string = X_draw_string;
      return;
    }
  }
  if ((vga.char_width < 8) || (vga.char_width > 9))
    vga.char_width = 8;
  font_width = vga.char_width;
  if ((vga.char_height < 2) || (vga.char_height > 32))
    vga.char_height = 16;
  font_height = vga.char_height;
}


/*
 * Load the mouse cursor shapes.
 */
void load_cursor_shapes()
{
  Colormap cmap;
  XColor fg, bg;
  Font cfont;

  /* Use a white on black cursor as the background is normally dark. */
  cmap = DefaultColormap(display, screen);
  XParseColor(display, cmap, "white", &fg);
  XParseColor(display, cmap, "black", &bg);

  cfont = XLoadFont(display, "cursor");
  if (!cfont) {
    error("X: Unable to open font \"cursor\", aborting!\n");
    leavedos(99);
  }

#if CONFIG_X_MOUSE
  X_standard_cursor = XCreateGlyphCursor(
    display, cfont, cfont, 
    XC_top_left_arrow, XC_top_left_arrow+1, &fg, &bg
  );

  X_mouse_nocursor = create_invisible_cursor();
#endif /* CONFIG_X_MOUSE */

  XUnloadFont(display, cfont);
}


/*
 * Makes an invisible cursor (to be used when no cursor should be drawn).
 */
Cursor create_invisible_cursor()
{
  Pixmap mask;
  Cursor cursor;
  static char map[16] = {0};

  mask = XCreatePixmapFromBitmapData(display, mainwindow, map, 1, 1, 0, 0, 1);
  cursor = XCreatePixmapCursor(display, mask, mask, (XColor *) map, (XColor *) map, 0, 0);
  XFreePixmap(display, mask);
  return cursor;
}


/*
 * Draw the cursor (nothing in graphics modes, normal if we have focus,
 * rectangle otherwise).
 */
void X_draw_text_cursor(int x, int y, Bit8u attr, int start, int end, Boolean focus)
{
  int cstart, cend;

  /* no hardware cursor emulation in graphics modes (erik@sjoerd) */
  if(vga.mode_class == GRAPH) return;

  set_gc_attr(attr);
  if(!focus) {
    XDrawRectangle(
      display, mainwindow, gc,
      (shift_x + x * font_width) * w_x_res / x_res,
      (shift_y + y * font_height) * w_y_res / y_res,
      (font_width - 1) * w_x_res / x_res,
      (font_height - 1) * w_y_res / y_res
      );
  }
  else {
    cstart = start * font_height / 16;
    cend = end * font_height / 16;
    XFillRectangle(
      display, mainwindow, gc,
      (shift_x + x * font_width) * w_x_res / x_res,
      (shift_y + y * font_height + cstart) * w_y_res / y_res,
      font_width * w_x_res / x_res,
      (cend - cstart + 1) * w_y_res / y_res
    );
  }
}


/*
 * Redraw the cursor if it's necessary.
 * Do nothing in graphics modes.
 */
void X_update_cursor() 
{
  /* no hardware cursor emulation in graphics modes (erik@sjoerd) */
  if(vga.mode_class == GRAPH) return;
  update_cursor();
}


#if CONFIG_X_MOUSE
/* 
 * DANG_BEGIN_FUNCTION set_mouse_position
 *
 * description:
 * Place the mouse on the right position.
 * 
 * DANG_END_FUNCTION
 */ 
void set_mouse_position(int x, int y)
{
  int dx, dy, center_x, center_y, x0, y0;

  x -= shift_x;
  y -= shift_y;
  x0 = x;
  y0 = y;

  if(grab_active) {
    center_x = w_x_res >> 1;
    center_y = w_y_res >> 1;
    dx = dy = 0;
    if (x == center_x && y == center_y) return;  /* assume pointer warp event */
    x = x - center_x + mouse_x;
    y = y - center_y + mouse_y;
    x0 = x; y0 = y;
    XWarpPointer(display, None, mainwindow, 0, 0, 0, 0, center_x, center_y);
    dx = x - mouse_x;
    dy = (y - mouse_y) * 2;
    mouse_move_relative(dx, dy);
  } else if(snap_X) {
    /*
     * win31 cursor snap kludge, we temporary force the DOS cursor to the
     * upper left corner (0,0). If we after that release snapping,
     * normal X-events will move the cursor to the exact position. (Hans)
     */
     dx = dy = x0 = y0 = 0;
     dx = -3 * x_res; dy = -6 * y_res;		/* enough ??? -- sw */
     mouse_move_relative(dx, dy);
     snap_X--;
  } else {
    mouse_move_absolute(x, y, w_x_res, w_y_res);
  }

  mouse_x = x0;
  mouse_y = y0;
}


/*
 * Appears to set the mouse buttons. Called from the
 * X event handler.
 */
void set_mouse_buttons(int state)
{
  mouse_move_buttons(state & Button1Mask, state & Button2Mask, state & Button3Mask);
}
#endif /* CONFIG_X_MOUSE */

/* do this even without CONFIG_XMOUSE; otherwise xterm mouse
   events may be tried... */
static int X_mouse_init(void)
{
  mouse_t *mice = &config.mouse;
  if (Video != &Video_X || !mice->intdrv)
    return FALSE;
  mice->type = MOUSE_X;
  mice->use_absolute = 1;
  mice->native_cursor = 0;	/* we have the X cursor */
  m_printf("MOUSE: X Mouse being set\n");
  return TRUE;
}

struct mouse_client Mouse_X =  {
  "X",          /* name */
  X_mouse_init, /* init */
  NULL,		/* close */
  NULL,		/* run */
  X_set_mouse_cursor /* set_cursor */
};

#if CONFIG_X_SELECTION

/*
 * Convert X coordinate to column, with bounds checking.
 */
int x_to_col(int x)
{
  int col = x_res*(x-shift_x)/font_width/w_x_res;
  if (font == NULL) {
    li = vga.text_height;
    co = vga.text_width;
  }
  if (col < 0)
    col = 0;
  else if (col >= co)
    col = co-1;
  return(col);
}


/*
 * Convert Y coordinate to row, with bounds checking.
 */
int y_to_row(int y)
{
  int row = y_res*(y-shift_y)/font_height/w_y_res;
  if (font == NULL) {
    li = vga.text_height;
    co = vga.text_width;
  }
  if (row < 0)
    row = 0;
  else if (row >= li)
    row = li-1;
  return(row);
}

/*
 * Send selection data to other window.
 */
void send_selection(Time time, Window requestor, Atom target, Atom property)
{
	XEvent e;

	e.xselection.type = SelectionNotify;
	e.xselection.selection = XA_PRIMARY;
	e.xselection.requestor = requestor;
	e.xselection.time = time;
	e.xselection.serial = 0;
	e.xselection.send_event = True;
	e.xselection.target = target;
	e.xselection.property = property;

	if (sel_text == NULL) {
		X_printf("X: Window 0x%lx requested selection, but it's empty!\n",   
			(unsigned long) requestor);
		e.xselection.property = None;
	}
	else if (target == targets[TARGETS_ATOM]) {
		X_printf("X: selection: TARGETS\n");
		XChangeProperty(display, requestor, property, XA_ATOM, 32,
			PropModeReplace, (char *)targets, NUM_TARGETS);
	}
	else if (target == targets[TIMESTAMP_ATOM]) {
		X_printf("X: timestamp atom %lu\n", sel_time);
		XChangeProperty(display, requestor, property, XA_INTEGER, 32,
			PropModeReplace, (char *)&sel_time, 1);
	}
	else if (target == targets[STRING_TARGET] ||
		 target == targets[COMPOUND_TARGET] ||
		 target == targets[UTF8_TARGET] ||
		 target == targets[TEXT_TARGET]) {
		X_printf("X: selection: %s\n",sel_text);
		XChangeProperty(display, requestor, property, target, 8, PropModeReplace, 
			sel_text, strlen(sel_text));
		X_printf("X: Selection sent to window 0x%lx as %s\n", 
			(unsigned long) requestor, XGetAtomName(display, target));
	}
	else
	{
		e.xselection.property = None;
		X_printf("X: Window 0x%lx requested unknown selection format %ld %s\n",
			(unsigned long) requestor, (unsigned long) target,
			 XGetAtomName(display, target));
	}
	XSendEvent(display, requestor, False, 0, &e);
}

#endif /* CONFIG_X_SELECTION */


/*
 * Doesn't really belong here!
 * It will go into a separate file. -- sw
 */

#define KDOS_CLOSE_MSG	1

void kdos_recv_msg(unsigned char *buf)
{
  fprintf(stderr, "got Msg %d\n", buf[0]);
}

void kdos_send_msg(unsigned char *buf)
{
  XEvent e;

  if(!kdos_client) return;

  e.xclient.type = ClientMessage;
  e.xclient.serial = 0;
  e.xclient.display = display;
  e.xclient.window = parentwindow;
  e.xclient.message_type = comm_atom;
  e.xclient.format = 8;
  memcpy(e.xclient.data.b, buf, sizeof e.xclient.data.b);

  XSendEvent(display, parentwindow, False, 0, &e);
}

void kdos_close_msg()
{
  unsigned char m[20] = { KDOS_CLOSE_MSG, };
  kdos_send_msg(m);
}
