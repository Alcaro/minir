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

#ifndef __VIDEO_CONTEXT_DRIVER_H
#define __VIDEO_CONTEXT_DRIVER_H

#include <boolean.h>
#include <retro_common_api.h>

#include "video_driver.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

RETRO_BEGIN_DECLS

#ifndef MAX_EGLIMAGE_TEXTURES
#define MAX_EGLIMAGE_TEXTURES 32
#endif

enum gfx_ctx_api
{
   GFX_CTX_NONE = 0,
   GFX_CTX_OPENGL_API,
   GFX_CTX_OPENGL_ES_API,
   GFX_CTX_DIRECT3D8_API,
   GFX_CTX_DIRECT3D9_API,
   GFX_CTX_OPENVG_API,
   GFX_CTX_VULKAN_API
};
    
enum display_metric_types
{
   DISPLAY_METRIC_NONE = 0,
   DISPLAY_METRIC_MM_WIDTH,
   DISPLAY_METRIC_MM_HEIGHT,
   DISPLAY_METRIC_DPI
};

enum display_flags
{
   GFX_CTX_FLAGS_NONE = 0,
   GFX_CTX_FLAGS_GL_CORE_CONTEXT,
   GFX_CTX_FLAGS_CUSTOMIZABLE_SWAPCHAIN_IMAGES
};

typedef void (*gfx_ctx_proc_t)(void);

typedef struct gfx_ctx_driver
{
   /* The opaque pointer is the underlying video driver data (e.g. gl_t for
    * OpenGL contexts). Although not advised, the context driver is allowed
    * to hold a pointer to it as the context never outlives the video driver.
    *
    * The context driver is responsible for it's own data.*/
   void* (*init)(void *video_driver);
   void (*destroy)(void *data);

   /* Which API to bind to. */
   bool (*bind_api)(void *video_driver, enum gfx_ctx_api,
         unsigned major, unsigned minor);

   /* Sets the swap interval. */
   void (*swap_interval)(void *data, unsigned);

   /* Sets video mode. Creates a window, etc. */
   bool (*set_video_mode)(void*, unsigned, unsigned, bool);

   /* Gets current window size.
    * If not initialized yet, it returns current screen size. */
   void (*get_video_size)(void*, unsigned*, unsigned*);

   void (*get_video_output_size)(void*, unsigned*, unsigned*);

   void (*get_video_output_prev)(void*);

   void (*get_video_output_next)(void*);

   bool (*get_metrics)(void *data, enum display_metric_types type,
         float *value);

   /* Translates a window size to an aspect ratio.
    * In most cases this will be just width / height, but
    * some contexts will better know which actual aspect ratio is used.
    * This can be NULL to assume the default behavior.
    */
   float (*translate_aspect)(void*, unsigned, unsigned);

   /* Asks driver to update window title (FPS, etc). */
   void (*update_window_title)(void*);

   /* Queries for resize and quit events.
    * Also processes events. */
   void (*check_window)(void*, bool*, bool*,
         unsigned*, unsigned*, unsigned);

   /* Acknowledge a resize event. This is needed for some APIs. 
    * Most backends will ignore this. */
   bool (*set_resize)(void*, unsigned, unsigned);

   /* Checks if window has input focus. */
   bool (*has_focus)(void*);

   /* Should the screensaver be suppressed? */
   bool (*suppress_screensaver)(void *data, bool enable);
    
   /* Checks if context driver has windowed support. */
   bool (*has_windowed)(void*);

   /* Swaps buffers. VBlank sync depends on 
    * earlier calls to swap_interval. */
   void (*swap_buffers)(void*);

   /* Most video backends will want to use a certain input driver.
    * Checks for it here. */
   void (*input_driver)(void*, const input_driver_t**, void**);

   /* Wraps whatever gl_proc_address() there is.
    * Does not take opaque, to avoid lots of ugly wrapper code. */
   gfx_ctx_proc_t (*get_proc_address)(const char*);

   /* Returns true if this context supports EGLImage buffers for 
    * screen drawing and was initalized correctly. */
   bool (*image_buffer_init)(void*, const video_info_t*);

   /* Writes the frame to the EGLImage and sets image_handle to it. 
    * Returns true if a new image handle is created.
    * Always returns true the first time it's called for a new index. 
    * The graphics core must handle a change in the handle correctly. */
   bool (*image_buffer_write)(void*, const void *frame, unsigned width,
         unsigned height, unsigned pitch, bool rgb32,
         unsigned index, void **image_handle);

   /* Shows or hides mouse. Can be NULL if context doesn't 
    * have a concept of mouse pointer. */
   void (*show_mouse)(void *data, bool state);

   /* Human readable string. */
   const char *ident;

   uint32_t (*get_flags)(void *data);

   void     (*set_flags)(void *data, uint32_t flags);

   /* Optional. Binds HW-render offscreen context. */
   void (*bind_hw_render)(void *data, bool enable);

   /* Optional. Gets base data for the context which is used by the driver.
    * This is mostly relevant for graphics APIs such as Vulkan
    * which do not have global context state. */
   void *(*get_context_data)(void *data);

   /* Optional. Makes driver context (only GLX right now)
    * active for this thread. */
   void (*make_current)(bool release);
} gfx_ctx_driver_t;

typedef struct gfx_ctx_flags
{
   uint32_t flags;
} gfx_ctx_flags_t;

typedef struct gfx_ctx_size
{
   bool *quit;
   bool *resize;
   unsigned *width;
   unsigned *height;
} gfx_ctx_size_t;

typedef struct gfx_ctx_mode
{
   unsigned width;
   unsigned height;
   bool fullscreen;
} gfx_ctx_mode_t;

typedef struct gfx_ctx_metrics
{
   enum display_metric_types type;
   float *value;
} gfx_ctx_metrics_t;

typedef struct gfx_ctx_aspect
{
   float *aspect;
   unsigned width;
   unsigned height;
} gfx_ctx_aspect_t;

typedef struct gfx_ctx_image
{
   const void *frame;
   unsigned width;
   unsigned height;
   unsigned pitch;
   unsigned index;
   bool rgb32;
   void **handle;
} gfx_ctx_image_t;

typedef struct gfx_ctx_input
{
   const input_driver_t **input;
   void **input_data;
} gfx_ctx_input_t;

typedef struct gfx_ctx_proc_address
{
   const char *sym;
   retro_proc_address_t addr;
} gfx_ctx_proc_address_t;

typedef struct gfx_ctx_ident
{
   const char *ident;
} gfx_ctx_ident_t;

extern const gfx_ctx_driver_t gfx_ctx_osmesa;
extern const gfx_ctx_driver_t gfx_ctx_sdl_gl;
extern const gfx_ctx_driver_t gfx_ctx_x_egl;
extern const gfx_ctx_driver_t gfx_ctx_wayland;
extern const gfx_ctx_driver_t gfx_ctx_x;
extern const gfx_ctx_driver_t gfx_ctx_d3d;
extern const gfx_ctx_driver_t gfx_ctx_drm;
extern const gfx_ctx_driver_t gfx_ctx_mali_fbdev;
extern const gfx_ctx_driver_t gfx_ctx_vivante_fbdev;
extern const gfx_ctx_driver_t gfx_ctx_android;
extern const gfx_ctx_driver_t gfx_ctx_ps3;
extern const gfx_ctx_driver_t gfx_ctx_wgl;
extern const gfx_ctx_driver_t gfx_ctx_videocore;
extern const gfx_ctx_driver_t gfx_ctx_bbqnx;
extern const gfx_ctx_driver_t gfx_ctx_cgl;
extern const gfx_ctx_driver_t gfx_ctx_cocoagl;
extern const gfx_ctx_driver_t gfx_ctx_emscripten;
extern const gfx_ctx_driver_t gfx_ctx_opendingux_fbdev;
extern const gfx_ctx_driver_t gfx_ctx_khr_display;
extern const gfx_ctx_driver_t gfx_ctx_null;

/**
 * video_context_driver_init_first:
 * @data                    : Input data.
 * @ident                   : Identifier of graphics context driver to find.
 * @api                     : API of higher-level graphics API.
 * @major                   : Major version number of higher-level graphics API.
 * @minor                   : Minor version number of higher-level graphics API.
 * @hw_render_ctx           : Request a graphics context driver capable of
 *                            hardware rendering?
 *
 * Finds first suitable graphics context driver and initializes.
 *
 * Returns: graphics context driver if found, otherwise NULL.
 **/
const gfx_ctx_driver_t *video_context_driver_init_first(void *data, const char *ident,
      enum gfx_ctx_api api, unsigned major, unsigned minor, bool hw_render_ctx);

bool video_context_driver_check_window(gfx_ctx_size_t *size_data);

bool video_context_driver_find_prev_driver(void);

bool video_context_driver_find_next_driver(void);

bool video_context_driver_init_image_buffer(const video_info_t *data);

bool video_context_driver_write_to_image_buffer(gfx_ctx_image_t *img);

bool video_context_driver_get_video_output_prev(void);

bool video_context_driver_get_video_output_next(void);

bool video_context_driver_bind_hw_render(bool *enable);

void video_context_driver_make_current(bool restore);

bool video_context_driver_set(const gfx_ctx_driver_t *data);

void video_context_driver_destroy(void);

bool video_context_driver_update_window_title(void);

bool video_context_driver_swap_buffers(void);

bool video_context_driver_focus(void);

bool video_context_driver_get_video_output_size(gfx_ctx_size_t *size_data);

bool video_context_driver_swap_interval(unsigned *interval);

bool video_context_driver_get_proc_address(gfx_ctx_proc_address_t *proc);

bool video_context_driver_suppress_screensaver(bool *bool_data);

bool video_context_driver_get_ident(gfx_ctx_ident_t *ident);

bool video_context_driver_set_video_mode(gfx_ctx_mode_t *mode_info);

bool video_context_driver_set_resize(gfx_ctx_mode_t *mode_info);

bool video_context_driver_get_video_size(gfx_ctx_mode_t *mode_info);

bool video_context_driver_get_context_data(void *data);

bool video_context_driver_show_mouse(bool *bool_data);

void video_context_driver_set_data(void *data);

bool video_context_driver_get_flags(gfx_ctx_flags_t *flags);

bool video_context_driver_set_flags(gfx_ctx_flags_t *flags);

bool video_context_driver_get_metrics(gfx_ctx_metrics_t *metrics);

bool video_context_driver_translate_aspect(gfx_ctx_aspect_t *aspect);

bool video_context_driver_input_driver(gfx_ctx_input_t *inp);

bool video_context_driver_has_windowed(void);

void video_context_driver_free(void);

RETRO_END_DECLS

#endif
