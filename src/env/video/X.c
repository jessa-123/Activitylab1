/* 
 * (C) Copyright 1992, ..., 1998 the "DOSEMU-Development-Team".
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
 * $Id: X.c,v 1.13 1995/05/06 16:25:24 root Exp root $
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
 * -- sw (Steffen.Winterfeldt@itp.uni-leipzig.de)
 *
 * 1997/04/21: Changed the interface to vgaemu - basically we do
 * all image conversions in remap.c now. Added & enhanced support
 * for 8/15/16/24/32 bit displays (you can resize the graphics
 * window now).
 * Palette updates in text-modes work; the code is not
 * perfect yet (no screen update is done), so the change will
 * take effect only after some time...
 * -- sw (Steffen.Winterfeldt@itp.uni-leipzig.de)
 *
 * 1997/06/06: Fixed the code that was supposed to turn off MIT-SHM for
 * network connections. Thanks to Leonid V. Kalmankin <leonid@cs.msu.su>
 * for finding the bug and testing the fix.
 * -- sw (Steffen.Winterfeldt@itp.uni-leipzig.de)
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

/* when grabbing all mouse events, grab keyboard too */
#undef ENABLE_KEYBOARD_GRAB

/* "fine tuning" option for X_update_screen */
#define MAX_UNCHANGED	3

/* not yet -- sw */
#undef HAVE_DGA

#define CONFIG_X_SELECTION 1

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

#include "emu.h"
#include "bios.h"
#include "video.h"
#include "memory.h"
#include "remap.h"
#include "vgaemu.h"
#include "X.h"

#ifdef HAVE_MITSHM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#endif

#ifdef HAVE_DGA
#include <X11/extensions/xf86dga.h>
#endif

#if CONFIG_X_MOUSE
#include "mouse.h"
#include <time.h>
#endif

#if CONFIG_X_SELECTION
#include "screen.h"
#endif

#if CONFIG_X_SPEAKER
#include "speaker.h"
#endif


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* Kludge for incorrect ASCII 0 char in vga font. */
#define XCHAR(w) ((u_char)CHAR(w)?(u_char)CHAR(w):(u_char)' ')

#if CONFIG_X_SELECTION
#define SEL_ACTIVE(w) (visible_selection && ((w) >= sel_start) && ((w) <= sel_end))
#define SEL_ATTR(attr) (((attr) >> 4) ? (attr) :(((attr) & 0x0f) << 4))
/* #define SEL_ATTR(attr) ((((attr) >> 4) & 0x0f) + (((attr) << 4) & 0xf0)) */
#define XATTR(w) (SEL_ACTIVE(w) ? SEL_ATTR(ATTR(w)) : ATTR(w))
#else
#define XATTR(w) (ATTR(w))
#endif

#define XREAD_WORD(w) ((XATTR(w)<<8)|XCHAR(w))


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
typedef struct { unsigned char r, g, b; } c_cube;


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* base/init/config.c */
extern int dosemu_argc;
extern char **dosemu_argv;

/* base/keyboard/keyb_X.c */
extern void X_process_key(XKeyEvent *);

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

static int (*OldXErrorHandler)(Display *, XErrorEvent *) = NULL;

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

Display *display;		/* used in base/keyboard/keyb_X_keycode.c */
static int screen;
static Visual *visual;
static Window rootwindow, mainwindow;

static XImage *ximage;		/* Used as a buffer for the X-server */

static Cursor X_standard_cursor;

#if CONFIG_X_MOUSE
static Cursor X_mouse_cursor;
static Cursor X_mouse_nocursor;
static time_t X_mouse_change_time;
static int X_mouse_last_on;
static int snap_X = 0;
#endif

static GC gc;
static Font vga_font;
static Atom proto_atom = None, delete_atom = None;
static XFontStruct *font = NULL;
static int font_width, font_height, font_shift;
static int prev_cursor_row = -1, prev_cursor_col = -1;
static ushort prev_cursor_shape = NO_CURSOR;
static int blink_state = 1;
static int blink_count = 8;

#if CONFIG_X_SELECTION
static int sel_start_row = -1, sel_end_row = -1, sel_start_col, sel_end_col;
static unsigned short *sel_start = NULL, *sel_end = NULL;
static u_char *sel_text = NULL;
static Atom compound_text_atom = None;
static Boolean doing_selection = FALSE, visible_selection = FALSE;
#endif

static Boolean have_focus = FALSE;
static Boolean is_mapped = FALSE;

static int cmap_colors = 0;		/* entries in colormaps: {text,graphics}_cmap */
static Colormap text_cmap = 0, graphics_cmap = 0;
static Boolean have_shmap = FALSE;
static unsigned long text_colors[16];
static int text_col_stats[16] = {0};

static RemapObject remap_obj;
static ColorSpaceDesc X_csd;
static int have_true_color;
static unsigned dac_bits;		/* the bits visible to the dosemu app, not the real number */
static int x_res, y_res;		/* emulated window size in pixels */
static int w_x_res, w_y_res;		/* actual window size */
static unsigned ximage_bits_per_pixel;
static unsigned ximage_mode;
static int remap_features;
static vga_emu_update_type veut;
static int remap_src_modes = 0;
static vgaemu_display_type X_screen;

static int grab_active = 0;
static char *grab_keystring = "Home";
static int grab_keysym = NoSymbol;
static int mouse_x = 0, mouse_y = 0;

static int X_map_mode = -1;
static int X_unmap_mode = -1;


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
static void X_remap_init(void);
static void X_remap_done(void);

/* error/event handler */
static int NewXErrorHandler(Display *, XErrorEvent *);
void X_handle_events(void);		/* used in arch/linux/async/signal.c */

/* interface to xmode.exe */
int X_change_config(unsigned, void *);	/* used in base/async/int.c */

/* colormap related stuff */
static void graphics_cmap_init(void);
static int MakePrivateColormap(void);
static ColorSpaceDesc MakeSharedColormap(void);
static int try_cube(unsigned long *, c_cube *);

/* palette/color update stuff */
static void refresh_palette(void);
static void refresh_text_palette(void);
static void refresh_truecolor(void);
static void refresh_private_palette(void);
static void get_approx_color(XColor *, Colormap, int);

/* ximage/drawing related stuff */
static void create_ximage(void);
static void destroy_ximage(void);
static void put_ximage(int, int, int, int, unsigned, unsigned);
static void resize_ximage(unsigned, unsigned);

/* video mode set/modify stuff */
static int X_set_videomode(int, int, int);
static int X_setmode(int, int, int, int);
static void X_modify_mode(void);
void X_set_textsize(int, int);
static void X_resize_text_screen(void);

/* screen update/redraw functions */
static void X_reset_redraw_text_screen(void);
static void X_redraw_text_screen(void);
static int X_update_screen(void);
static int X_update_text_screen(void);
static int X_update_graphics_screen(void);
static void set_gc_attr(Bit8u);
static void X_draw_string(int, int, char *, int, Bit8u);

/* text mode init stuff (font/cursor) */
static void load_text_font(void);
static void load_cursor_shapes(void);
static Cursor create_invisible_cursor(void);

/* text mode cursor manipulation stuff */
static void draw_cursor(int, int);
static void redraw_cursor(void);
static void X_update_cursor(void);
void X_blink_cursor(void);		/* used in arch/linux/async/signal.c */
static inline void restore_cell(int, int);

/* something to do with mouse cursor */
void X_change_mouse_cursor(void);	/* used in base/mouse/mouse.c */

#if CONFIG_X_MOUSE
/* mouse related code */
static void set_mouse_position(int, int);
static void set_mouse_buttons(int);
#endif

#if CONFIG_X_SELECTION
static int x_to_col(int);
static int y_to_row(int);
static void calculate_selection(void);
static void clear_visible_selection(void);
static void clear_selection_data(void);
static void clear_if_in_selection(void);
static void start_selection(int, int);
static void extend_selection(int, int);
static void start_extend_selection(int, int);
static void save_selection_data(void);
static void end_selection(void);
static void send_selection(Time, Window, Atom, Atom);
#endif

#if USE_SCROLL_QUEUE
static void X_scroll(int, int, int, int, int, Bit8u);
static void do_scroll(void);
#endif


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Interface to video emulation.
 * used in env/video/video.c
 */
struct video_system Video_X = 
{
   0,                /* is_mapped */
   X_init,         
   X_close,      
   X_set_videomode,      
   X_update_screen,
   X_update_cursor
};


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * DANG_BEGIN_FUNCTION X_close
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
  XWMHints wmhint;
  XSetWindowAttributes attr;
  char *display_name; 

  X_printf("X: X_init\n");

  co = 80; li = 25;
  set_video_bios_size();		/* make it stick */

  /* Open X connection. */
  display_name = config.X_display ? config.X_display : getenv("DISPLAY");
  display = XOpenDisplay(display_name);
  if(display == NULL) {
    error("X: Can't open display \"%s\"\n", display_name ? display_name : "");
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

  text_cmap = DefaultColormap(display, screen);		/* always use the global palette */
  graphics_cmap_init();				/* graphics modes are more sophisticated */

  X_remap_init();		/* init graphics mode support */
  if(!remap_src_modes) {
    error("X: No graphics modes supported on this type of screen!\n");
  }

  load_text_font();

  w_x_res = x_res = co * font_width;
  w_y_res = y_res = li * font_height;

  mainwindow = XCreateSimpleWindow(
    display, rootwindow,
    0, 0,			/* position */
    w_x_res, w_y_res,		/* size */
    0, 0,			/* border width & color */
    text_colors[0]		/* background color */
  );

  X_printf("X: X_init: screen = %d, root = 0x%x, mainwindow = 0x%x\n",
    screen, (unsigned) rootwindow, (unsigned) mainwindow
  );

  load_cursor_shapes();

  gcv.font = vga_font;
  gc = XCreateGC(display, mainwindow, GCFont, &gcv);

  attr.event_mask =
    KeyPressMask | KeyReleaseMask | KeymapStateMask |
    ButtonPressMask | ButtonReleaseMask |
    EnterWindowMask | LeaveWindowMask | PointerMotionMask |
    ExposureMask | StructureNotifyMask | FocusChangeMask |
    ColormapChangeMask;

  attr.cursor = X_standard_cursor;

  XChangeWindowAttributes(display, mainwindow, CWEventMask | CWCursor, &attr);

  XStoreName(display, mainwindow, config.X_title);
  XSetIconName(display, mainwindow, config.X_icon_name);
  xch.res_name  = "XDosEmu";
  xch.res_class = "XDosEmu";

  wmhint.window_group = mainwindow;
  wmhint.flags = WindowGroupHint;
  XSetWMProperties(display, mainwindow, NULL, NULL, 
    dosemu_argv, dosemu_argc, NULL, &wmhint, &xch);

#if CONFIG_X_SELECTION
  /* Get atom for COMPOUND_TEXT type. */
  compound_text_atom = XInternAtom(display, "COMPOUND_TEXT", False);
#endif
  /* Delete-Window-Message black magic copied from xloadimage. */
  proto_atom  = XInternAtom(display, "WM_PROTOCOLS", False);
  delete_atom = XInternAtom(display, "WM_DELETE_WINDOW", False);
  if ((proto_atom != None) && (delete_atom != None)) {
    XChangeProperty(display, mainwindow,
      proto_atom, XA_ATOM, 32,
      PropModePrepend, (char *) &delete_atom, 1
    );
  }

  /* don't map window if set */
  if(getenv("DOSEMU_HIDE_WINDOW") == NULL) XMapWindow(display, mainwindow);

  /* initialize VGA emulator */
  X_screen.src_modes = remap_src_modes;
  X_screen.bits = X_csd.bits;
  X_screen.bytes = X_csd.bytes;
  X_screen.r_mask = X_csd.r_mask;
  X_screen.g_mask = X_csd.g_mask;
  X_screen.b_mask = X_csd.b_mask;
  X_screen.r_shift = X_csd.r_shift;
  X_screen.g_shift = X_csd.g_shift;
  X_screen.b_shift = X_csd.b_shift;
  X_screen.r_bits = X_csd.r_bits;
  X_screen.g_bits = X_csd.g_bits;
  X_screen.b_bits = X_csd.b_bits;
  if(vga_emu_init(&X_screen)) {
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

#ifdef CONFIG_X_SPEAKER
  /* turn off the sound, and */
  speaker_off();
  /* reset the speaker to it's default */
  register_speaker(NULL, NULL, NULL);
#endif

  XUnloadFont(display, vga_font);
  XDestroyWindow(display, mainwindow);

  destroy_ximage();

  vga_emu_done();
  
  if(graphics_cmap) XFreeColormap(display, graphics_cmap);

  XFreeGC(display, gc);

  if(X_csd.pixel_lut != NULL) { free(X_csd.pixel_lut); X_csd.pixel_lut = NULL; }

  X_remap_done();

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

  xi = XCreateImage(
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


/*
 * Initialize the interface between the VGA emulator and X.
 * Check if X's color depth is supported.
 */
void X_remap_init()
{
  set_remap_debug_msg(stderr);

  if(have_true_color) {
    switch(ximage_bits_per_pixel) {
      case  1: ximage_mode = MODE_TRUE_1_MSB; break;
      case 15: ximage_mode = MODE_TRUE_15; break;
      case 16: ximage_mode = MODE_TRUE_16; break;
      case 24: ximage_mode = MODE_TRUE_24; break;
      case 32: ximage_mode = MODE_TRUE_32; break;
      default: ximage_mode = MODE_UNSUP;
    }
  }
  else {
    switch(ximage_bits_per_pixel) {
      case  8: ximage_mode = have_shmap ? MODE_TRUE_8 : MODE_PSEUDO_8; break;
      default: ximage_mode = MODE_UNSUP;
    }
  }

  remap_features = 0;
  if(config.X_lin_filt) remap_features |= RFF_LIN_FILT;
  if(config.X_bilin_filt) remap_features |= RFF_BILIN_FILT;

  remap_obj = remap_init(0, ximage_mode, 0);
  remap_src_modes = remap_obj.supported_src_modes;
  remap_done(&remap_obj);
}


/*
 * Free resources associated with remap_obj.
 */
void X_remap_done()
{
  remap_done(&remap_obj);
}


/*
 * This function provides an interface to reconfigure parts
 * of X and the VGA emulation during a DOSEMU session.
 * It is used by the xmode.exe program that comes with DOSEMU.
 */
int X_change_config(unsigned item, void *buf)
{
  int err = 0;
  XFontStruct *xfont;
  XGCValues gcv;

  X_printf("X: X_change_config: item = %d, buffer = 0x%x\n", item, (unsigned) buf);

  switch(item) {

    case X_CHG_TITLE:
      X_printf("X: X_change_config: win_name = %s\n", (char *) buf);
      XStoreName(display, mainwindow, buf);
      break;

    case X_CHG_FONT:
      xfont = XLoadQueryFont(display, (char *) buf);
      if(xfont == NULL) {
        X_printf("X: X_change_config: font \"%s\" not found\n", (char *) buf);
      }
      else {
        if(xfont->min_bounds.width != xfont->max_bounds.width) {
          X_printf("X: X_change_config: font \"%s\" is not monospaced\n", (char *) buf);
          XFreeFont(display, xfont);
        }
        else {
          if(font != NULL) XFreeFont(display, font);
          font = xfont;
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
 * DANG_BEGIN_FUNCTION X_handle_events
 *
 * description:
 * Handle pending X events (called from SIGALRM handler).
 *
 * DANG_END_FUNCTION
 */
void X_handle_events()
{
   static int busy = 0;
   XEvent e;
   unsigned resize_width = w_x_res, resize_height = w_y_res, resize_event = 0;

   /*  struct timeval currenttime;
  struct timezone tz;*/


  /* Can't we turn off the SIGALRMs altogether while in here,
     and, more importantly, reset the interval from the start
     when we leave? */

#if 0
   gettimeofday(&currenttime, &tz);
   printf("ENTER: %10d %10d - ",currenttime.tv_sec,currenttime.tv_usec);
   printf(XPending(display)?"Event.":"ALARM!");
#endif

   if (busy)
     {
       fprintf(stderr, " - busy.\n");
       return;
     }
     
   busy=1;
   

#if CONFIG_X_MOUSE
   {
     static int lastingraphics = 0;
     if(vga.mode_class == GRAPH) {
       if(! lastingraphics) {
         lastingraphics = 1;
#ifndef DEBUG_MOUSE_POS
         XDefineCursor(display, mainwindow, X_mouse_nocursor);
#else
         XDefineCursor(display, mainwindow, X_standard_cursor);
#endif
       }
     }
     else {
       if(lastingraphics) {
         lastingraphics = 0;
         XDefineCursor(display, mainwindow, X_standard_cursor);
       }
     }
   }
   if(X_mouse_change_time != 0 &&
      time(NULL) > X_mouse_change_time) {
     X_mouse_change_time = 0;
     if(mouse.cursor_on != X_mouse_last_on) {
       if (mouse.cursor_on != 0)
       XDefineCursor(display, mainwindow, X_standard_cursor);
       else
       XDefineCursor(display, mainwindow, X_mouse_cursor);

       X_mouse_last_on = mouse.cursor_on;
     }
   }
#if 0 /* maybe we need to trigger mouse snapping for win31 more often,
       * But on my machine, it seems not to be necessary.
       * (Hans, 970118)
       */
   if (snap_X) set_mouse_position(0,0);
#endif
#endif

  while (XPending(display) > 0) 
    {
      XNextEvent(display,&e);

      switch(e.type) 
	{
       case Expose:  
	  X_printf("X: expose event\n");
	  if(vga.mode_class == TEXT) {
	    if(e.xexpose.count == 0) X_redraw_text_screen();
	  }
	  else {	/* GRAPH */
	    if(!resize_event) put_ximage(
	      e.xexpose.x, e.xexpose.y,
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
	  have_focus = TRUE;
	  redraw_cursor();
	  break;

	case FocusOut:
	  X_printf("X: focus out\n");
	  have_focus = FALSE;
	  blink_state = TRUE;
	  blink_count = config.X_blinkrate;
	  redraw_cursor();
	  break;

	case DestroyNotify:
	  error("X: Window got destroyed!\n");
	  leavedos(99);
	  break;
	  
	case ClientMessage:
	  /* If we get a client message which has the value of the delete
	   * atom, it means the window manager wants us to die.
	   */
	  if (e.xclient.data.l[0] == delete_atom) {
	    error("X: Got delete message!\n");
	    /* XXX - Is it ok to call this from a SIGALRM handler? */
	    leavedos(0);    
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
          if((e.xkey.state & ControlMask) && (e.xkey.state & Mod1Mask) && grab_keysym != NoSymbol)
            if(XKeycodeToKeysym(display, e.xkey.keycode, 0) == grab_keysym) {
              if(grab_active ^= 1) {
                X_printf("X: mouse grab activated\n");
#ifdef ENABLE_KEYBOARD_GRAB
                XGrabKeyboard(display, mainwindow, True, GrabModeAsync, GrabModeAsync, CurrentTime);
#endif
                XGrabPointer(display, mainwindow, True, PointerMotionMask | ButtonPressMask | ButtonReleaseMask,
                  GrabModeAsync, GrabModeAsync, mainwindow,  None, CurrentTime);
              }
              else {
                X_printf("X: mouse grab released\n");
                XUngrabPointer(display, CurrentTime);
#ifdef ENABLE_KEYBOARD_GRAB
                XUngrabKeyboard(display, CurrentTime);
#endif
              }
              break;
            }

	case KeyRelease:
/* 
      Clears the visible selection if the cursor is inside the selection
*/
#if CONFIG_X_SELECTION
	  if (visible_selection)
	    clear_if_in_selection();
#endif
	  X_process_key(&e.xkey);
	  break;
    /* A keyboard mapping has been changed (e.g., with xmodmap). */
	case MappingNotify:  
	  X_printf("X: MappingNotify event\n");
	  XRefreshKeyboardMapping(&e.xmapping);
	  break;

/* Mouse events */
#if CONFIG_X_MOUSE  
	case ButtonPress:
#if CONFIG_X_SELECTION
	if (vga.mode_class == TEXT) {
	  if (e.xbutton.button == Button1)
	    start_selection(e.xbutton.x, e.xbutton.y);
	  else if (e.xbutton.button == Button3)
	    start_extend_selection(e.xbutton.x, e.xbutton.y);
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
	      end_selection();
	      break;
	    case Button2 :
	      X_printf("X: mouse Button2Release\n");
	      scr_request_selection(display,mainwindow,e.xbutton.time,
				    e.xbutton.x,
				    e.xbutton.y);
	      break;
	    }
#endif /* CONFIG_X_SELECTION */
	  set_mouse_buttons(e.xbutton.state&~(0x80<<e.xbutton.button));
	  break;
	  
	case MotionNotify:
#if CONFIG_X_SELECTION
	  if (doing_selection)
	    extend_selection(e.xmotion.x, e.xmotion.y);
#endif /* CONFIG_X_SELECTION */
	  set_mouse_position(e.xmotion.x, e.xmotion.y);
	  break;
	  
	case EnterNotify:
	  /* Here we trigger the kludge to snap in the cursor
	   * drawn by win31 and align it with the X-cursor.
	   * (Hans)
	   */
	  if(!grab_active) snap_X=3;
	  X_printf("X: Mouse entering window\n");
	  set_mouse_position(e.xcrossing.x, e.xcrossing.y);
	  set_mouse_buttons(e.xcrossing.state);
	  break;
	  
	case LeaveNotify:                   /* Should this do anything? */
	  X_printf("X: Mouse leaving window\n");
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

#endif /* CONFIG_X_SELECTION */
/* some weirder things... */
	}
    }

    if(ximage != NULL && resize_event) {
      if(ximage->width == resize_width && ximage->height == resize_height) resize_event = 0;
    }

    if(resize_event) {
      resize_ximage(resize_width, resize_height);
      dirty_all_video_pages();
      X_update_screen();
    }

  busy = 0;
#if CONFIG_X_MOUSE  
  mouse_event();
#endif  

#if 0
  gettimeofday(&currenttime, &tz);
  printf("\nLEAVE: %10d %10d\n",currenttime.tv_sec,currenttime.tv_usec);
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
 * Update the active colormap to reflect the current DAC entries.
 */
void refresh_palette()
{
  if(vga.mode_class == TEXT) {
    refresh_text_palette();
  }
  else {
    if(have_true_color || have_shmap)
      refresh_truecolor();
    else
      refresh_private_palette();
  }
}


/*
 * Reallocate color cells if a text color has changed. If no
 * free color cell is left, choose an approximate color.
 *
 * Note: Redraws the *entire* screen if at least one color has changed.
 */
void refresh_text_palette()
{
  DAC_entry col[16];
  XColor xc;
  int i, j, k, shift = 16 - dac_bits;
  int read_cmap = 1;

  if(vga.pixel_size > 4) {
    X_printf("X: refresh_text_palette: invalid color size - no updates made\n");
    return;
  }

  j = changed_vga_colors(col);

  for(k = 0; k < j; k++) {
    xc.flags = DoRed | DoGreen | DoBlue;
    xc.pixel = text_colors[i = col[k].index];
    xc.red   = col[k].r << shift;
    xc.green = col[k].g << shift;
    xc.blue  = col[k].b << shift;

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

  if(j) X_redraw_text_screen();
}


/*
 * Update the displayed image to match the current DAC entries.
 * Will redraw the *entire* screen if at least one color has changed.
 */
void refresh_truecolor()
{
  DAC_entry col[256];
  Boolean colchanged = False;
  int i, j;

  j = changed_vga_colors(col);

  for(i = 0; i < j; i++) {
    colchanged |=
    remap_obj.palette_update(&remap_obj, col[i].index, dac_bits, col[i].r, col[i].g, col[i].b);
  }
  if(colchanged) dirty_all_video_pages();
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
void put_ximage(int src_x, int src_y, int dest_x, int dest_y, unsigned width, unsigned height)
{
#ifdef HAVE_MITSHM
  if(shm_ok)
    XShmPutImage(display, mainwindow, gc, ximage, src_x, src_y, dest_x, dest_y, width, height, True);
  else
#endif /* HAVE_MITSHM */
    XPutImage(display, mainwindow, gc, ximage, src_x, src_y, dest_x, dest_y, width, height);
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
 * DANG_BEGIN_FUNCTION X_set_videomode
 *
 * description:
 * This is the interface function called by the video subsystem
 * to set a video mode.
 *
 * NOTE: The actual mode is taken from the global variable "video_mode".
 *
 * DANG_END_FUNCTION
 */

int X_set_videomode(int mode_class, int text_width, int text_height)
{
  return X_setmode(video_mode, text_width, text_height, 1);
}


/* 
 * DANG_BEGIN_FUNCTION X_setmode
 *
 * description:
 * Set the video mode.
 * If init_vga is zero, this will only reinitialize the current mode.
 * The other arguments are ignored in this case.
 *
 * DANG_END_FUNCTION
 */
int X_setmode(int mode, int text_width, int text_height, int init_vga)
{
  XSizeHints sh;
  int X_mode_type;
#ifdef X_USE_BACKING_STORE
  XSetWindowAttributes xwa;
#endif

  if(init_vga) {	/* tell vgaemu we're going to another mode */
    if(!vga_emu_setmode(mode, text_width, text_height)) return 0;
  }
  else {
    vga.reconfig.re_init = 0;
  }
                                
  X_printf("X: X_setmode: %svideo_mode 0x%x (%s), size %d x %d (%d x %d pixel)\n",
    init_vga ? "" : "re-init ",
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
  if(vga.mode_class == TEXT) {
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

    w_x_res = x_res = vga.text_width * font_width;
    w_y_res = y_res = vga.text_height * font_height;

    /* We use the X font! Own font in future? */

    sh.width = sh.min_width = sh.max_width = w_x_res;
    sh.height = sh.min_height = sh.max_height = w_y_res;

    sh.flags = PSize | PMinSize | PMaxSize;
    XSetNormalHints(display, mainwindow, &sh);
    XSync(display, False);

    XResizeWindow(display, mainwindow, w_x_res, w_y_res);

    XSetForeground(display, gc, text_colors[0]);
    XFillRectangle(display, mainwindow, gc, 0, 0, w_x_res, w_y_res);
  }
  else {	/* GRAPH */

    if(!have_true_color) {
      XSetWindowColormap(display, mainwindow, graphics_cmap);
    }

    dac_bits = vga.dac.bits;

    w_x_res = x_res = vga.width;
    w_y_res = y_res = vga.height;

    /* 320x200 modes */
    if(vga.width == 320 && vga.height == 200) {
      w_x_res *= config.X_mode13fact;
      w_y_res *= config.X_mode13fact;
    }

    /* 640x200 modes */
    if(vga.width == 640 && vga.height == 200) {
      w_y_res *= 2;
    }

    if(config.X_winsize_x > 0 && config.X_winsize_y > 0) {
      w_x_res = config.X_winsize_x;
      w_y_res = config.X_winsize_y;
    }

    if(config.X_aspect_43) {
      w_y_res = (w_x_res * 3) >> 2;
    }

    remap_done(&remap_obj);
    switch(vga.mode_type) {
      case CGA:
        X_mode_type = vga.pixel_size == 2 ? MODE_CGA_2 : MODE_CGA_1; break;
      case HERC:
        X_mode_type = MODE_HERC; break;
      case PL1:
        X_mode_type = MODE_VGA_1; break;
      case PL2:
        X_mode_type = MODE_VGA_2; break;
      case PL4:
        X_mode_type = MODE_VGA_4; break;
      case P8:
        X_mode_type = MODE_PSEUDO_8; break;
      case P15:
        X_mode_type = MODE_TRUE_15; break;
      case P16:
        X_mode_type = MODE_TRUE_16; break;
      case P24:
        X_mode_type = MODE_TRUE_24; break;
      case P32:
        X_mode_type = MODE_TRUE_32; break;
      default:
        X_mode_type = 0;
    }
    remap_obj = remap_init(X_mode_type, ximage_mode, remap_features);
    *remap_obj.dst_color_space = X_csd;
    adjust_gamma(&remap_obj, config.X_gamma);

    if(!(remap_obj.state & ROS_SCALE_ALL)) {
      if((remap_obj.state & ROS_SCALE_2) && !(remap_obj.state & ROS_SCALE_1)) {
        w_x_res = x_res << 1;
        w_y_res = y_res << 1;
      }
      else {
        w_x_res = x_res;
        w_y_res = y_res;
      }
    }

    create_ximage();

    remap_obj.dst_image = ximage->data;
    remap_obj.src_resize(&remap_obj, vga.width, vga.height, vga.scan_len);
    remap_obj.dst_resize(&remap_obj, w_x_res, w_y_res, ximage->bytes_per_line);

    if(!(remap_obj.state & (ROS_SCALE_ALL | ROS_SCALE_1 | ROS_SCALE_2))) {
      error("X: X_setmode: vide mode 0x%02x not supported on this screen\n", mode);
    }

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

    veut.base = vga.mem.base;
    veut.max_max_len = 0;
    veut.max_len = 0;
    veut.display_start = 0;
    veut.display_end = vga.scan_len * vga.height;
    veut.update_gran = 0;
    veut.update_pos = veut.display_start;

    XSetNormalHints(display, mainwindow, &sh);
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
 * Modify the current graphics mode.
 * Currently used to turn on/off chain4 addressing, change
 * the VGA screen size, change the DAC size.
 */
static void X_modify_mode()
{
  RemapObject tmp_ro;

  if(vga.reconfig.mem) {
    if(remap_obj.src_mode == MODE_PSEUDO_8 || remap_obj.src_mode == MODE_VGA_X) {

      tmp_ro = remap_init(vga.mem.planes == 1 ? MODE_PSEUDO_8 : MODE_VGA_X, ximage_mode, remap_features);
      *tmp_ro.dst_color_space = X_csd;
      tmp_ro.dst_image = ximage->data;
      tmp_ro.src_resize(&tmp_ro, vga.width, vga.height, vga.scan_len);
      tmp_ro.dst_resize(&tmp_ro, w_x_res, w_y_res, ximage->bytes_per_line);

      if(!(tmp_ro.state & (ROS_SCALE_ALL | ROS_SCALE_1 | ROS_SCALE_2))) {
        x_msg("X_modify_mode: no memory config change of current graphics mode supported\n");
        remap_done(&tmp_ro);
      }
      else {
        x_msg("X_modify_mode: chain4 addressing turned %s\n", vga.mem.planes == 1 ? "on" : "off");
        remap_done(&remap_obj);
        remap_obj = tmp_ro;
      }

      dirty_all_video_pages();
      /*
       * The new remap object does not yet know about our colors.
       * So we have to force an update. -- sw
       */
      dirty_all_vga_colors();

      vga.reconfig.mem =
      vga.reconfig.display =
      vga.reconfig.dac = 0;
    }
  }

  if(vga.reconfig.display) {
    remap_obj.src_resize(&remap_obj, vga.width, vga.height, vga.scan_len);
    x_msg(
      "X_modify_mode: geometry changed to %d x% d, scan_len = %d bytes\n",
      vga.width, vga.height, vga.scan_len
    );
    dirty_all_video_pages();
    vga.reconfig.display = 0;
  }

  if(vga.reconfig.dac) {
    vga.reconfig.dac = 0;
    dac_bits = vga.dac.bits;
    x_msg("X_modify_mode: DAC bits = %d\n", dac_bits);
  }

  veut.display_start = vga.display_start;
  veut.display_end = veut.display_start + vga.scan_len * vga.height;

  if(vga.reconfig.mem || vga.reconfig.display) {
    x_msg("X_modify_mode: failed to modify current graphics mode\n");
  }
}


/*
 * Set the text mode resolution.
 */
void X_set_textsize(int width, int height)
{
  X_printf("X: X_set_textsize: size = %d x %d\n", width, height);
  vga_emu_set_textsize(width, height);
  X_resize_text_screen();
}


/*
 * Resize the X display to the appropriate size.
 */
void X_resize_text_screen() 
{
  XSizeHints sh;
                                
  w_x_res = x_res = vga.text_width * font_width;
  w_y_res = y_res = vga.text_height * font_height;

  sh.width = sh.min_width = sh.max_width = w_x_res;
  sh.height = sh.min_height = sh.max_height = w_y_res;

  sh.flags = PSize | PMinSize | PMaxSize;
  XSetNormalHints(display, mainwindow, &sh);

  XResizeWindow(display, mainwindow, w_x_res, w_y_res);

  XSetForeground(display, gc, text_colors[0]);
  XFillRectangle(display, mainwindow, gc, 0, 0, w_x_res, w_y_res);

  X_redraw_text_screen();
}


/*
 * Sync prev_screen & screen_adr.
 */
void X_reset_redraw_text_screen()
{
  if(!is_mapped) return;

  prev_cursor_shape = NO_CURSOR; redraw_cursor();
  clear_scroll_queue();

  XFlush(display);

  MEMCPY_2UNIX(prev_screen, screen_adr, co * li * 2);
  clear_scroll_queue();
}


/*
 * Redraw the entire screen (in text modes). Used only for expose events.
 * It's graphics mode counterpart is a simple put_ximage() call
 * (cf. X_handle_events). Since we now use backing store in text modes,
 * this function will likely never be called (depending on X's configuration).
 *
 * Note: It redraws the *entire* screen.
 */
void X_redraw_text_screen()
{
  Bit16u *sp, *oldsp;
  u_char charbuff[MAX_COLUMNS], *bp;
  int x, y, start_x;
  Bit8u attr;

  if(!is_mapped) return;
  if(vga.mode_class == GRAPH) {
    x_msg("X_redraw_text_screen: Text refresh in graphics video mode?\n");
    return;
  }

  if(co > MAX_COLUMNS) {
    x_msg("X_redraw_text_screen: unable to handle %d colums\n", co);
    return;
  }

  x_deb(
    "X_redraw_text_screen: mode 0x%x (%d x %d), screen_adr = 0x%x\n",
    vga.mode, co, li, (unsigned) screen_adr
  );

  /* sp = (Bit16u *) (vga.mem.base + vga.display_start); */

  sp = screen_adr;
  oldsp = prev_screen;

  for(y = 0; y < li; y++) {
    x = 0;
    do {	/* scan in a string of chars of the same attribute */
      bp = charbuff; start_x = x; attr = XATTR(sp);

      do {	/* conversion of string to X */
        *oldsp++ = XREAD_WORD(sp); *bp++ = XCHAR(sp);
        sp++; x++;
      } while(XATTR(sp) == attr && x < co);

      X_draw_string(start_x, y, charbuff, x - start_x, attr);

    } while(x < co);
  }

  X_reset_redraw_text_screen();
}


/* 
 * DANG_BEGIN_FUNCTION X_update_screen
 *
 * description:
 * Update the part of the screen which has changed, in text mode
 * and in graphics mode. Usually called from the SIGALRM handler.
 *
 * X_update_screen returns 0 if nothing was updated, 1 if the whole
 * screen was updated, and 2 for a partial update.
 *
 * It is called in arch/linux/async/signal.c::SIGALRM_call() as part
 * of a struct video_system (see top of X.c) every 50 ms or
 * every 10 ms if 2 was returned, depending somewhat on various config
 * options as e.g. config.X_updatefreq and VIDEO_CHECK_DIRTY.
 * At least it is supposed to do that.
 *
 * DANG_END_FUNCTION
 *
 * Text and graphics updates are separate functions now; the code was
 * too messy. -- sw
 */
int X_update_screen()
{
  if(vga.config.video_off) {
    x_deb("X_update_screen: nothing done (video_off = 0x%x)\n", vga.config.video_off);
    return 1;
  }

  if(vga.reconfig.re_init) X_setmode(0, 0, 0, 0);

  return vga.mode_class == TEXT ? X_update_text_screen() : X_update_graphics_screen();
}


/*
 * Update the text screen.
 */
int X_update_text_screen()
{
  Bit16u *sp, *oldsp;
  u_char charbuff[MAX_COLUMNS], *bp;
  int x, y;	/* X and Y position of character being updated */
  int start_x, len, unchanged;
  Bit8u attr;

  static int yloop = -1;
  int lines;               /* Number of lines to redraw. */
  int numscan = 0;         /* Number of lines scanned. */
  int numdone = 0;         /* Number of lines actually updated. */

  if(!is_mapped) return 0;       /* no need to do anything... */

  refresh_palette();

#if USE_SCROLL_QUEUE
  do_scroll();
#endif
  
  /* The following determines how many lines it should scan at once,
   * since this routine is being called by sig_alrm.  If the entire
   * screen changes, it often incurs considerable delay when this
   * routine updates the entire screen.  So the variable "lines"
   * contains the maximum number of lines to update at once in one
   * call to this routine.  This is set by the "updatelines" keyword
   * in /etc/dosemu.conf 
   */
	lines = config.X_updatelines;
	if (lines < 2) 
	  lines = 2;
	else if (lines > li)
	  lines = li;

  /* The highest priority is given to the current screen row for the
   * first iteration of the loop, for maximum typing response.  
   * If y is out of bounds, then give it an invalid value so that it
   * can be given a different value during the loop.
   */
	y = cursor_row;
	if ((y < 0) || (y >= li)) 
	  y = -1;

/*  X_printf("X: X_update_screen, co=%d, li=%d, yloop=%d, y=%d, lines=%d\n",
           co,li,yloop,y,lines);*/

  /* The following loop scans lines on the screen until the maximum number
   * of lines have been updated, or the entire screen has been scanned.
   */
	while ((numdone < lines) && (numscan < li)) 
	  {
    /* The following sets the row to be scanned and updated, if it is not
     * the first iteration of the loop, or y has an invalid value from
     * loop pre-initialization.
     */
	    if ((numscan > 0) || (y < 0)) 
	      {
		yloop = (yloop+1) % li;
		if (yloop == cursor_row)
		  yloop = (yloop+1) % li;
		y = yloop;
	      }
	    numscan++;
	    
	    sp = screen_adr + y*co;
	    oldsp = prev_screen + y*co;

	    x=0;
	    do 
	      {
        /* find a non-matching character position */
		start_x = x;
		while (XREAD_WORD(sp)==*oldsp) {
		  sp++; oldsp++; x++;
		  if (x==co) {
		    if (start_x == 0)
		      goto chk_cursor;
		    else
		      goto line_done;
		  }
		}
/* now scan in a string of changed chars of the same attribute.
   To keep the number of X calls (and thus the overhead) low,
   we tolerate a few unchanged characters (up to MAX_UNCHANGED in 
   a row) in the 'changed' string. 
*/
		bp = charbuff;
		start_x=x;
		attr=XATTR(sp);
		unchanged=0;         /* counter for unchanged chars */
		
		while(1) 
		  {
		    *bp++=XCHAR(sp);
		    *oldsp++ = XREAD_WORD(sp);
		    sp++; 
		    x++;

		    if ((XATTR(sp) != attr) || (x == co))
		      break;
		    if (XREAD_WORD(sp) == *oldsp) {
		      if (unchanged > MAX_UNCHANGED) 
			break;
		      unchanged++;
		    }
		    else
		      unchanged=0;
		  } 
		len=x-start_x-unchanged;

                /* ok, we've got the string now send it to the X server */

                X_draw_string(start_x, y, charbuff, len, attr);

		if ((prev_cursor_row == y) && 
		    (prev_cursor_col >= start_x) && 
		    (prev_cursor_col < start_x+len))
		  {
		    prev_cursor_shape=NO_CURSOR;  
/* old cursor was overwritten */
		  }
	      } 
	    while(x<co);
line_done:
/* Increment the number of successfully updated lines counter */
	    numdone++;

chk_cursor:
/* update the cursor. We do this here to avoid the cursor 'running behind'
       when using a fast key-repeat.
*/
	    if (y == cursor_row)
	      X_update_cursor();
	  }
	XFlush(display);

/*	X_printf("X: X_update_screen: %d lines updated\n",numdone);*/
	
	if (numdone) 
	  {
	    if (numscan==li)
	      return 1;     /* changed, entire screen updated */
	    else
	      return 2;     /* changed, part of screen updated */
	  }
	else 
	  {
/* The updates look a bit cleaner when reset to top of the screen
 * if nothing had changed on the screen in this call to screen_restore
 */
	    yloop = -1;
	    return(1);
	  }
      
  return 0;
}


/*
 * Update the graphics screen.
 */
int X_update_graphics_screen()
{
  RectArea ra;
  int update_ret;
#ifdef DEBUG_SHOW_UPDATE_AREA
  static int dsua_fg_color = 0;
#endif		

  if(!is_mapped) return 0;		/* no need to do anything... */

  if(vga.reconfig.mem || vga.reconfig.display || vga.reconfig.dac) X_modify_mode();

  refresh_palette();

  if(vga.display_start != veut.display_start) {
    veut.display_start = vga.display_start;
    veut.display_end = veut.display_start + vga.scan_len * vga.height;
    dirty_all_video_pages();
  }

  /*
   * You can now set the maximum update length, like this:
   *
   * veut.max_max_len = 20000;
   *
   * vga_emu_update() will return -1, if this limit is exceeded and there
   * are still invalid pages; `max_max_len' is only a rough estimate, the
   * actual upper limit will vary (might even be 25000 in the above
   * example).
   */

  veut.max_len = veut.max_max_len;

  while((update_ret = vga_emu_update(&veut)) > 0) {
    remap_obj.src_image = veut.base + veut.display_start;
    ra = remap_obj.remap_mem(&remap_obj, veut.update_start - veut.display_start, veut.update_len);

#ifdef DEBUG_SHOW_UPDATE_AREA
    XSetForeground(display, gc, dsua_fg_color++);
    XFillRectangle(display, mainwindow, gc, ra.x, ra.y, ra.width, ra.height);
    XSync(display, False);
#endif

    put_ximage(ra.x, ra.y, ra.x, ra.y, ra.width, ra.height);

    x_deb("X_update_graphics_screen: func = %s, display_start = 0x%04x, write_plane = %d, start %d, len %u, win (%d,%d),(%d,%d)\n",
      remap_obj.remap_func_name, vga.display_start, vga.mem.write_plane,
      veut.update_start, veut.update_len, ra.x, ra.y, ra.width, ra.height
    );
  }

  return update_ret < 0 ? 2 : 1;
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
 * Draw a text string.
 * The attribute is the VGA color/mono text attribute.
 */
void X_draw_string(int x, int y, char *text, int len, Bit8u attr)
{
  x_deb2(
    "X_draw_string: %d chars at (%d, %d), attr = 0x%02x\n",
    len, x, y, (unsigned) attr
  );

  set_gc_attr(attr);

  XDrawImageString(
    display, mainwindow, gc,
    font_width * x,
    font_height * y + font_shift,
    text,
    len
  );

  if(vga.mode_type == TEXT_MONO && (attr == 0x01 || attr == 0x09 || attr == 0x89)) {
    XDrawLine(
      display, mainwindow, gc,
      font_width * x, font_height * y + font_shift,
      font_width * (x + len) - 1, font_height * y + font_shift
    );
  }
}


/*
 * Load the main text font. Try first the user specified font, then
 * vga, then 9x15 and finally fixed. If none of these exists and
 * is monospaced, give up (shouldn't happen).
 */
void load_text_font()
{
  const char *fonts[] = {config.X_font, "vga", "9x15", "fixed", NULL}, **p = fonts;

  while(1) {
    if((font = XLoadQueryFont(display, *p)) == NULL) {
      error("X: Unable to open font \"%s\"", *p);
    }
    else if(font->min_bounds.width != font->max_bounds.width) {
      error("X: Font \"%s\" isn't monospaced", *p);
      XFreeFont(display, font);
      font = NULL;
    }
    else {
      font_width = font->max_bounds.width;
      font_height = font->max_bounds.ascent + font->max_bounds.descent;
      font_shift = font->max_bounds.ascent;
      vga_font = font->fid;
      X_printf("X: Using font \"%s\", size = %d x %d\n", *p, font_width, font_height);
      break;     
    }  

    if(p[1] != NULL) {
      error(", trying \"%s\"...\n", p[1]);
      p++;
    }
    else {
      /* If the user doesn't have "fixed", he's in big trouble anyway. */
      error(", aborting!\n");
      leavedos(99);
    }  
  }
}


/*
 * Load the mouse cursor shapes.
 */
void load_cursor_shapes()
{
  Colormap cmap;
  XColor fg, bg;
  Font cfont;
#if CONFIG_X_MOUSE  
  Font decfont;
#endif  

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

  /*
   * IMHO, the DEC cursor font looks nicer, but if it's not there, 
   * use the standard cursor font. 
   */
  decfont = XLoadFont(display, "decw$cursor");
  if(!decfont) {
    X_mouse_cursor = XCreateGlyphCursor(
      display, cfont, cfont, XC_hand2, XC_hand2+1, &bg, &fg
    );
  } 
  else {
    X_mouse_cursor = XCreateGlyphCursor(display, decfont, decfont, 2, 3, &fg, &bg);
    XUnloadFont(display, decfont);  
  }

  X_mouse_nocursor = create_invisible_cursor();

  X_mouse_change_time = 0;
  X_mouse_last_on = -1;
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
void draw_cursor(int x, int y)
{  
  int cstart, cend;

  /* no hardware cursor emulation in graphics modes (erik@sjoerd) */
  if(vga.mode_class == GRAPH) return;

  set_gc_attr(XATTR(screen_adr + y * co + x));
  if(!have_focus) {
    XDrawRectangle(
      display, mainwindow, gc,
      x * font_width, y * font_height,
      font_width - 1, font_height - 1
    );
  }
  else if(blink_state) {
    cstart = CURSOR_START(cursor_shape) * font_height / 16;
    cend = CURSOR_END(cursor_shape) * font_height / 16;
    XFillRectangle(
      display, mainwindow, gc,
      x * font_width, y * font_height + cstart,
      font_width, cend - cstart + 1
    );
  }
}


/*
 * Move cursor to a new position (and erase the old cursor).
 * Do nothing in graphics modes.
 */
void redraw_cursor()
{
  /* no hardware cursor emulation in graphics modes (erik@sjoerd) */
  if(vga.mode_class == GRAPH || !is_mapped) return;

  if(prev_cursor_shape != NO_CURSOR)
    restore_cell(prev_cursor_col, prev_cursor_row);

  if(cursor_shape != NO_CURSOR)
    draw_cursor(cursor_col, cursor_row);

  XFlush(display);

  prev_cursor_row = cursor_row;
  prev_cursor_col = cursor_col;
  prev_cursor_shape = cursor_shape;
}


/*
 * Redraw the cursor if it's necessary.
 * Do nothing in graphics modes.
 */
void X_update_cursor() 
{
  /* no hardware cursor emulation in graphics modes (erik@sjoerd) */
  if(vga.mode_class == GRAPH) return;
   
  if(
    cursor_row != prev_cursor_row ||
    cursor_col != prev_cursor_col ||
    cursor_shape != prev_cursor_shape
  ) {
    redraw_cursor();
  }
}


/*
 * Blink the cursor. Called from SIGALRM handler.
 * Do nothing in graphics modes.
 */
void X_blink_cursor()
{
  /* no hardware cursor emulation in graphics modes (erik@sjoerd) */
  if(vga.mode_class == GRAPH) return;

  if(!have_focus || --blink_count) return;

  blink_count = config.X_blinkrate;
  blink_state = !blink_state;

  if(cursor_shape != NO_CURSOR) {
    if(
      cursor_row != prev_cursor_row ||
      cursor_col != prev_cursor_col
    ) {
      restore_cell(prev_cursor_col, prev_cursor_row);
      prev_cursor_row = cursor_row;
      prev_cursor_col = cursor_col;
      prev_cursor_shape = cursor_shape;
    }

    if(blink_state)
      draw_cursor(cursor_col, cursor_row);
    else
      restore_cell(cursor_col, cursor_row);
  }
}


/*
 * Restore a character cell (used to remove the cursor).
 * Do nothing in graphics modes.
 */
inline void restore_cell(int x, int y)
{
  Bit16u *sp = screen_adr + y * co + x, *oldsp = prev_screen + y * co + x;
  u_char c = XCHAR(sp);

  /* no hardware cursor emulation in graphics modes (erik@sjoerd) */
  if(vga.mode_class == GRAPH) return;

  *oldsp = XREAD_WORD(sp);
  X_draw_string(x, y, &c, 1, XATTR(sp));
}


/*
 * DANG_BEGIN_FUNCTION X_change_mouse_cursor
 *
 * description:
 * This function seems to be called each screen_update :(
 * It is called in base/mouse/mouse.c:mouse_cursor(int) a lot for
 * show and hide (changes the mouse cursor shape).
 *
 * DANG_END_FUNCTION
 *
 * DANG_FIXME Use a delayline, or something smarter in X_change_mouse_cursor
 *
 * Well, what is this function *really* for? -- sw
 */  
void X_change_mouse_cursor()
{
#if CONFIG_X_MOUSE
  /* Yes, this is illogical, but blame the author of the mouse code. 
   * mouse.cursor_on is zero if the cursor is on! 
   */
  if(X_mouse_change_time == 0) X_mouse_change_time = time(NULL);
#endif    
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
  int dx, dy, center_x, center_y, x0 = x, y0 = y;

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
    dy = y - mouse_y;
    mouse_move_relative(dx, dy);
  } else if(snap_X) {
    /*
     * win31 cursor snap kludge, we temporary force the DOS cursor to the
     * upper left corner (0,0). If we after that release snapping,
     * normal X-events will move the cursor to the exact position. (Hans)
     */
     dx = dy = x0 = y0 = 0;
     dx = -3 * x_res; dy = -3 * y_res;		/* enough ??? -- sw */
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


#if CONFIG_X_SELECTION
/*
 * Convert X coordinate to column, with bounds checking.
 */
int x_to_col(int x)
{
  int col = x/font_width;
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
  int row = y/font_height;
  if (row < 0)
    row = 0;
  else if (row >= li)
    row = li-1;
  return(row);
}


/*
 * Calculate sel_start and sel_end pointers from sel_start | end_col | row.
 */
void calculate_selection()
{
  if ((sel_end_row < sel_start_row) || 
    ((sel_end_row == sel_start_row) && (sel_end_col < sel_start_col)))
  {
    sel_start = screen_adr+sel_end_row*co+sel_end_col;
    sel_end = screen_adr+sel_start_row*co+sel_start_col-1;
  }
  else
  {
    sel_start = screen_adr+sel_start_row*co+sel_start_col;
    sel_end = screen_adr+sel_end_row*co+sel_end_col-1;
  }
}


/*
 * Clear visible part of selection (but not the selection text buffer).
 */
void clear_visible_selection()
{
  sel_start_col = sel_start_row = sel_end_col = sel_end_row = 0;
  sel_start = sel_end = NULL;
  visible_selection = FALSE;
}


/*
 * Free the selection text buffer.
 */
void clear_selection_data()
{
  X_printf("X: Selection data cleared\n");
  if (sel_text != NULL)
  {
    free(sel_text);
    sel_text = NULL;
  }
  doing_selection = FALSE;
  clear_visible_selection();
}


/*
 * Check if we should clear selection.
 * Clear if cursor is in or before selected area.
*/
void clear_if_in_selection()
{
  X_printf("X:clear check selection , cursor at %d %d\n",
	   cursor_col,cursor_row);
  if (((sel_start_row <= cursor_row)&&(cursor_row <= sel_end_row)&&
       (sel_start_col <= cursor_col)&&(cursor_col <= sel_end_col)) ||
      /* cursor either inside selectio */
       (( cursor_row <= sel_start_row)&&(cursor_col <= sel_start_col)))
      /* or infront of selection */
    {
      X_printf("X:selection clear, key-event!\n");
      clear_visible_selection(); 
    }
}


/*
 * Start the selection process (mouse button 1 pressed).
 */
void start_selection(int x, int y)
{
  int col = x_to_col(x), row = y_to_row(y);
  sel_start_col = sel_end_col = col;
  sel_start_row = sel_end_row = row;
  calculate_selection();
  doing_selection = visible_selection = TRUE;
  X_printf("X:start selection , start %d %d, end %d %d\n",
	   sel_start_col,sel_start_row,sel_end_col,sel_end_row);
}


/*
 * Extend the selection (mouse motion).
 */
void extend_selection(int x, int y)
{
  int col = x_to_col(x), row = y_to_row(y);
  if ((sel_end_col == col) && (sel_end_row == row))
    return;
  sel_end_col = col;
  sel_end_row = row;
  calculate_selection();
  X_printf("X:extend selection , start %d %d, end %d %d\n",
	   sel_start_col,sel_start_row,sel_end_col,sel_end_row);
}


/*
 * Start extending the selection (mouse button 3 pressed).
 */
void start_extend_selection(int x, int y)
{
  /* Try to extend selection, visibility is handled by extend_selection*/
    doing_selection =  visible_selection = TRUE;  
    extend_selection(x, y);
  
}


/*
 * Copy the selected text to sel_text, and inform the X server about it.
 */
void save_selection_data()
{
  int col, row, col1, row1, col2, row2, line_start_col, line_end_col;
  u_char *p;
  if ((sel_end-sel_start) < 0)
  {
    visible_selection = FALSE;
    return;
  }
  row1 = (sel_start-screen_adr)/co;
  row2 = (sel_end-screen_adr)/co;
  col1 = (sel_start-screen_adr)%co;
  col2 = (sel_end-screen_adr)%co;
  
  /* Allocate space for text. */
  if (sel_text != NULL)
    free(sel_text);
  p = sel_text = malloc((row2-row1+1)*(co+1)+2);
  
  /* Copy the text data. */
  for (row = row1; (row <= row2); row++)
  {
    line_start_col = ((row == row1) ? col1 : 0);
    line_end_col = ((row == row2) ? col2 : co-1);
    for (col = line_start_col; (col <= line_end_col); col++)
    {
      *p++ = XCHAR(screen_adr+row*co+col);
    }
    /* Remove end-of-line spaces and add a newline. */
    if (col == co)
    { 
      p--;
      while ((*p == ' ') && (p > sel_text))
        p--;
      p++;
      *p++ = '\n';
    }
  }
  *p = '\0';
  if (strlen(sel_text) == 0)
    return;
    
  /* Inform the X server. */
  XSetSelectionOwner(display, XA_PRIMARY, mainwindow, CurrentTime);
  if (XGetSelectionOwner(display, XA_PRIMARY) != mainwindow)
  {
    X_printf("X: Couldn't get primary selection!\n");
    return;
}
  XChangeProperty(display, rootwindow, XA_CUT_BUFFER0, XA_STRING,
    8, PropModeReplace, sel_text, strlen(sel_text));

  X_printf("X: Selection, %d,%d->%d,%d, size=%d\n", 
    col1, row1, col2, row2, strlen(sel_text));
}


/*
 * End of selection (button released).
 */
void end_selection()
{
  if (!doing_selection)
    return;
  doing_selection = FALSE;
  save_selection_data();
}


/*
 * Send selection data to other window.
 */
void send_selection(Time time, Window requestor, Atom target, Atom property)
{
  int i;
  u_char *sel_text_latin;
  XEvent e;
  e.xselection.type = SelectionNotify;
  e.xselection.selection = XA_PRIMARY;
  e.xselection.requestor = requestor;
  e.xselection.time = time;
  if (sel_text == NULL) {
    X_printf("X: Window 0x%lx requested selection, but it's empty!\n",   
      (unsigned long) requestor);
    e.xselection.property = None;
  }
  else if ((target == XA_STRING) || (target == compound_text_atom)) {
    X_printf("X: selection (dos): %s\n",sel_text);   
    e.xselection.target = target;
    sel_text_latin = malloc(strlen(sel_text) + 1);
    strcpy(sel_text_latin, sel_text);
    for (i=0; i<strlen(sel_text_latin);i++)
      switch (sel_text_latin[i]) 
	{
	case 21 : /* � */
	  sel_text_latin[i] = 0xa7;
	  break;
	case 20 : /* � */
	  sel_text_latin[i] = 0xb6;
	  break;
	case 124 : /* � */
	  sel_text_latin[i] = 0xa6;
	  break;
	case 0x80 ... 0xff: 
          switch (config.term_charset) {
             case CHARSET_LATIN1:
                sel_text_latin[i]=dos_to_latin1[sel_text_latin[i] - 0x80];
                break;
             case CHARSET_LATIN2:
                sel_text_latin[i]=dos_to_latin2[sel_text_latin[i] - 0x80];
                break;
             case CHARSET_LATIN:
             default:
                sel_text_latin[i]=dos_to_latin[sel_text_latin[i] - 0x80];
                break;
          }
	} 
    X_printf("X: selection (latin): %s\n",sel_text_latin);  
    XChangeProperty(display, requestor, property, target, 8, PropModeReplace, 
      sel_text_latin, strlen(sel_text_latin));
    e.xselection.property = property;
    X_printf("X: Selection sent to window 0x%lx as %s\n", 
      (unsigned long) requestor, (target==XA_STRING)?"string":"compound_text");
    free(sel_text_latin);
  }
  else
  {
    e.xselection.property = None;
    X_printf("X: Window 0x%lx requested unknown selection format %ld\n",
      (unsigned long) requestor, (unsigned long) target);
  }
  XSendEvent(display, requestor, False, 0, &e);
}
#endif /* CONFIG_X_SELECTION */


#if USE_SCROLL_QUEUE
#if 0
#define Bit16u us
#define Boolean boolean
#endif

#error X does not do scroll queueing
  
/* XCopyArea can't be used if the window is obscured and backing store
 * isn't used!
 */
void X_scroll(int x, int y, int width, int height, int n, Bit8u attr)
{
  x*=font_width;
  y*=font_height;
  width*=font_width;
  height*=font_height;
  n*=font_height;
  
  XSetForeground(display,gc,text_colors[attr>>4]);
  
  if (n > 0) 
    {       /* scroll up */
      if (n>=height) 
	{
        n=height;
     }
      else 
	{
        height-=n;
	  XCopyArea(display,mainwindow,mainwindow,gc,x,y+n,width,height,x,y);
     }
     /*
      XFillRectangle(display,mainwindow,gc,x,y+height,width,n);
     */
  }
  else if (n < 0) 
    {  /* scroll down */
      if (-n>=height) 
	{
        n=-height;
     }
      else 
	{
        height+=n;
	  XCopyArea(display,mainwindow,mainwindow,gc,x,y,width,height,x,y-n);
     }
     /*
      XFillRectangle(display,mainwindow,gc,x,y,width,-n);
     */
  }
}


/* Process the scroll queue */
static void do_scroll(void) 
{
   struct scroll_entry *s;

   X_printf("X: do_scroll\n");
  while(s=get_scroll_queue()) 
    {
      if (s->n!=0) 
	{
         X_scroll(s->x0,s->y0,s->x1-s->x0+1,s->y1-s->y0+1,s->n,s->attr);
         Scroll(prev_screen,s->x0,s->y0,s->x1,s->y1,s->n,0xff);
      if ((prev_cursor_col >= s->x0) && (prev_cursor_col <= s->x1) &&
        (prev_cursor_row >= s->y0) && (prev_cursor_row <= s->y1))
         {
        prev_cursor_shape = NO_CURSOR;  /* Cursor was overwritten. */
         }
      }
   }
}
#endif   /* USE_SCROLL_QUEUE */


