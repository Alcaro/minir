/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef X11_COMMON_H__
#define X11_COMMON_H__

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <signal.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/xf86vmode.h>

#ifdef HAVE_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#include <boolean.h>

#include "../video_context_driver.h"

extern Window   g_x11_win;
extern Display *g_x11_dpy;
extern Colormap g_x11_cmap;
extern unsigned g_x11_screen;

void x11_save_last_used_monitor(Window win);
void x11_show_mouse(Display *dpy, Window win, bool state);
void x11_windowed_fullscreen(Display *dpy, Window win);
void x11_suspend_screensaver(Window win, bool enable);
bool x11_enter_fullscreen(Display *dpy, unsigned width,
      unsigned height, XF86VidModeModeInfo *desktop_mode);

void x11_exit_fullscreen(Display *dpy, XF86VidModeModeInfo *desktop_mode);
void x11_move_window(Display *dpy, Window win,
      int x, int y, unsigned width, unsigned height);

/* Set icon, class, default stuff. */
void x11_set_window_attr(Display *dpy, Window win);

#ifdef HAVE_XINERAMA
bool x11_get_xinerama_coord(Display *dpy, int screen,
      int *x, int *y, unsigned *w, unsigned *h);

unsigned x11_get_xinerama_monitor(Display *dpy,
      int x, int y, int w, int h);
#endif

bool x11_create_input_context(Display *dpy, Window win, XIM *xim, XIC *xic);
void x11_destroy_input_context(XIM *xim, XIC *xic);
void x11_handle_key_event(XEvent *event, XIC ic, bool filter);

bool x11_get_metrics(void *data,
      enum display_metric_types type, float *value);

void x11_check_window(void *data, bool *quit,
   bool *resize, unsigned *width, unsigned *height, unsigned frame_count);

void x11_get_video_size(void *data, unsigned *width, unsigned *height);

bool x11_has_focus(void *data);

bool x11_has_focus_internal(void *data);

bool x11_alive(void *data);

bool x11_connect(void);

void x11_update_window_title(void *data);

bool x11_input_ctx_new(bool true_full);

void x11_input_ctx_destroy(void);

void x11_window_destroy(bool fullscreen);

void x11_colormap_destroy(void);

void x11_install_quit_atom(void);

void x11_event_queue_check(XEvent *event);

#endif

