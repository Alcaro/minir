/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Higor Euripedes
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

#include <stdlib.h>
#include <string.h>

#include <retro_assert.h>
#include <gfx/scaler/scaler.h>
#include <retro_assert.h>

#include "SDL.h"

#include "../../configuration.h"
#include "../../driver.h"
#include "../../general.h"
#include "../../performance_counters.h"

#include "../video_frame.h"
#include "../video_context_driver.h"
#include "../font_driver.h"

#ifdef HAVE_X11
#include "../common/x11_common.h"
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "SDL_syswm.h"

typedef struct sdl_menu_frame
{
   bool active;
   SDL_Surface *frame;
   struct scaler_ctx scaler;

} sdl_menu_frame_t;

typedef struct sdl_video
{
   SDL_Surface *screen;
   bool quitting;

   void *font;
   const font_renderer_driver_t *font_driver;
   uint8_t font_r;
   uint8_t font_g;
   uint8_t font_b;

   struct scaler_ctx scaler;

   sdl_menu_frame_t menu;
} sdl_video_t;

static void sdl_gfx_free(void *data)
{
   sdl_video_t *vid = (sdl_video_t*)data;
   if (!vid)
      return;

   if (vid->menu.frame)
      SDL_FreeSurface(vid->menu.frame);

   SDL_QuitSubSystem(SDL_INIT_VIDEO);

   if (vid->font)
      vid->font_driver->free(vid->font);

   scaler_ctx_gen_reset(&vid->scaler);
   scaler_ctx_gen_reset(&vid->menu.scaler);

   free(vid);
}

static void sdl_init_font(sdl_video_t *vid, const char *font_path, unsigned font_size)
{
   int r, g, b;
   settings_t *settings = config_get_ptr();

   if (!settings->video.font_enable)
      return;

   if (!font_renderer_create_default((const void**)&vid->font_driver, &vid->font,
            *settings->path.font ? settings->path.font : NULL,
            settings->video.font_size))
   {
      RARCH_LOG("[SDL]: Could not initialize fonts.\n");
      return;
   }

   r = settings->video.msg_color_r * 255;
   g = settings->video.msg_color_g * 255;
   b = settings->video.msg_color_b * 255;

   r = (r < 0) ? 0 : (r > 255 ? 255 : r);
   g = (g < 0) ? 0 : (g > 255 ? 255 : g);
   b = (b < 0) ? 0 : (b > 255 ? 255 : b);

   vid->font_r = r;
   vid->font_g = g;
   vid->font_b = b;
}

static void sdl_render_msg(sdl_video_t *vid, SDL_Surface *buffer,
      const char *msg, unsigned width, unsigned height, const SDL_PixelFormat *fmt)
{
   int x, y, msg_base_x, msg_base_y;
   unsigned rshift, gshift, bshift;
   const struct font_atlas *atlas = NULL;
   settings_t *settings = config_get_ptr();

   if (!vid->font)
      return;

   atlas = vid->font_driver->get_atlas(vid->font);

   msg_base_x = settings->video.msg_pos_x * width;
   msg_base_y = (1.0f - settings->video.msg_pos_y) * height;

   rshift = fmt->Rshift;
   gshift = fmt->Gshift;
   bshift = fmt->Bshift;

   for (; *msg; msg++)
   {
      int glyph_width, glyph_height;
      int base_x, base_y, max_width, max_height;
      uint32_t *out      = NULL;
      const uint8_t *src = NULL;
      const struct font_glyph *glyph = vid->font_driver->get_glyph(vid->font, (uint8_t)*msg);
      if (!glyph)
         continue;

      glyph_width  = glyph->width;
      glyph_height = glyph->height;

      base_x = msg_base_x + glyph->draw_offset_x;
      base_y = msg_base_y + glyph->draw_offset_y;
      src    = atlas->buffer + glyph->atlas_offset_x 
         + glyph->atlas_offset_y * atlas->width;

      if (base_x < 0)
      {
         src -= base_x;
         glyph_width += base_x;
         base_x = 0;
      }

      if (base_y < 0)
      {
         src -= base_y * (int)atlas->width;
         glyph_height += base_y;
         base_y = 0;
      }

      max_width  = width - base_x;
      max_height = height - base_y;

      if (max_width <= 0 || max_height <= 0)
         continue;

      if (glyph_width > max_width)
         glyph_width = max_width;
      if (glyph_height > max_height)
         glyph_height = max_height;

      out = (uint32_t*)buffer->pixels + base_y * (buffer->pitch >> 2) + base_x;

      for (y = 0; y < glyph_height; y++, src += atlas->width, out += buffer->pitch >> 2)
      {
         for (x = 0; x < glyph_width; x++)
         {
            unsigned blend = src[x];
            unsigned out_pix = out[x];
            unsigned r = (out_pix >> rshift) & 0xff;
            unsigned g = (out_pix >> gshift) & 0xff;
            unsigned b = (out_pix >> bshift) & 0xff;

            unsigned out_r = (r * (256 - blend) + vid->font_r * blend) >> 8;
            unsigned out_g = (g * (256 - blend) + vid->font_g * blend) >> 8;
            unsigned out_b = (b * (256 - blend) + vid->font_b * blend) >> 8;
            out[x] = (out_r << rshift) | (out_g << gshift) | (out_b << bshift);
         }
      }

      msg_base_x += glyph->advance_x;
      msg_base_y += glyph->advance_y;
   }
}

static void sdl_gfx_set_handles(void)
{
   /* SysWMinfo headers are broken on OSX. */
#if defined(_WIN32) || defined(HAVE_X11)
   SDL_SysWMinfo info;
   SDL_VERSION(&info.version);

   if (SDL_GetWMInfo(&info) != 1)
      return;

#if defined(_WIN32)
   video_driver_display_type_set(RARCH_DISPLAY_WIN32);
   video_driver_display_set(0);
   video_driver_window_set((uintptr_t)info.window);
#elif defined(HAVE_X11)
   video_driver_display_type_set(RARCH_DISPLAY_X11);
   video_driver_display_set((uintptr_t)info.info.x11.display);
   video_driver_window_set((uintptr_t)info.info.x11.window);
#endif
#endif
}

static void *sdl_gfx_init(const video_info_t *video, const input_driver_t **input, void **input_data)
{
   unsigned full_x, full_y;
   const SDL_VideoInfo *video_info = NULL;
   sdl_video_t *vid = NULL;
   settings_t *settings = config_get_ptr();

#ifdef HAVE_X11
   XInitThreads();
#endif

   if (SDL_WasInit(0) == 0)
   {
      if (SDL_Init(SDL_INIT_VIDEO) < 0)
         return NULL;
   }
   else if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
      return NULL;

   vid = (sdl_video_t*)calloc(1, sizeof(*vid));
   if (!vid)
      return NULL;

   video_info = SDL_GetVideoInfo();
   retro_assert(video_info);
   full_x = video_info->current_w;
   full_y = video_info->current_h;
   RARCH_LOG("[SDL]: Detecting desktop resolution %ux%u.\n", full_x, full_y);

   if (!video->fullscreen)
      RARCH_LOG("[SDL]: Creating window @ %ux%u\n", video->width, video->height);

   vid->screen = SDL_SetVideoMode(video->width, video->height, 32,
         SDL_HWSURFACE | SDL_HWACCEL | SDL_DOUBLEBUF | (video->fullscreen ? SDL_FULLSCREEN : 0));

   /* We assume that SDL chooses ARGB8888.
    * Assuming this simplifies the driver *a ton*.
    */

   if (!vid->screen)
   {
      RARCH_ERR("[SDL]: Failed to init SDL surface: %s\n", SDL_GetError());
      goto error;
   }

   if (video->fullscreen)
      SDL_ShowCursor(SDL_DISABLE);

   sdl_gfx_set_handles();

   if (input && input_data)
   {
      void *sdl_input = input_sdl.init();

      if (sdl_input)
      {
         *input = &input_sdl;
         *input_data = sdl_input;
      }
      else
      {
         *input = NULL;
         *input_data = NULL;
      }
   }

   sdl_init_font(vid, settings->path.font, settings->video.font_size);

   vid->scaler.scaler_type = video->smooth ? SCALER_TYPE_BILINEAR : SCALER_TYPE_POINT;
   vid->scaler.in_fmt  = video->rgb32 ? SCALER_FMT_ARGB8888 : SCALER_FMT_RGB565;
   vid->scaler.out_fmt = SCALER_FMT_ARGB8888;

   vid->menu.scaler = vid->scaler;
   vid->menu.scaler.scaler_type = SCALER_TYPE_BILINEAR;

   vid->menu.frame = SDL_ConvertSurface(vid->screen, vid->screen->format, vid->screen->flags | SDL_SRCALPHA);

   if (!vid->menu.frame)
   {
      RARCH_ERR("[SDL]: Failed to init menu surface: %s\n", SDL_GetError());
      goto error;
   }

   return vid;

error:
   sdl_gfx_free(vid);
   return NULL;
}

static void sdl_gfx_check_window(sdl_video_t *vid)
{
   SDL_Event event;

   SDL_PumpEvents();
   while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_QUITMASK))
   {
      if (event.type != SDL_QUIT)
         continue;

      vid->quitting = true;
      break;
   }
}

static bool sdl_gfx_frame(void *data, const void *frame, unsigned width,
      unsigned height, uint64_t frame_count,
      unsigned pitch, const char *msg)
{
   char                       buf[128] = {0};
   static struct retro_perf_counter sdl_scale = {0};
   sdl_video_t                    *vid = (sdl_video_t*)data;

   if (!frame)
      return true;

   if (SDL_MUSTLOCK(vid->screen))
      SDL_LockSurface(vid->screen);

   performance_counter_init(&sdl_scale, "sdl_scale");
   performance_counter_start(&sdl_scale);

   video_frame_scale(
         &vid->scaler,
         vid->screen->pixels,
         frame,
         vid->scaler.in_fmt,
         vid->screen->w,
         vid->screen->h,
         vid->screen->pitch,
         width,
         height,
         pitch);
   performance_counter_stop(&sdl_scale);

   if (vid->menu.active)
      SDL_BlitSurface(vid->menu.frame, NULL, vid->screen, NULL);

   if (msg)
      sdl_render_msg(vid, vid->screen, msg, vid->screen->w, vid->screen->h, vid->screen->format);

   if (SDL_MUSTLOCK(vid->screen))
      SDL_UnlockSurface(vid->screen);

   if (video_monitor_get_fps(buf, sizeof(buf), NULL, 0))
      SDL_WM_SetCaption(buf, NULL);

   SDL_Flip(vid->screen);

   return true;
}

static void sdl_gfx_set_nonblock_state(void *data, bool state)
{
   (void)data; /* Can SDL even do this? */
   (void)state;
}

static bool sdl_gfx_alive(void *data)
{
   sdl_video_t *vid = (sdl_video_t*)data;
   sdl_gfx_check_window(vid);
   return !vid->quitting;
}

static bool sdl_gfx_focus(void *data)
{
   (void)data;
   return (SDL_GetAppState() & (SDL_APPINPUTFOCUS | SDL_APPACTIVE)) == (SDL_APPINPUTFOCUS | SDL_APPACTIVE);
}

static bool sdl_gfx_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;
#ifdef HAVE_X11
   if (video_driver_display_type_get() == RARCH_DISPLAY_X11)
   {
      x11_suspend_screensaver(video_driver_window_get(), enable);
      return true;
   }
#endif

   return false;
}

static bool sdl_gfx_has_windowed(void *data)
{
   (void)data;

   /* TODO - implement. */
   return true;
}

static void sdl_gfx_viewport_info(void *data, struct video_viewport *vp)
{
   sdl_video_t *vid = (sdl_video_t*)data;
   vp->x = vp->y = 0;
   vp->width  = vp->full_width  = vid->screen->w;
   vp->height = vp->full_height = vid->screen->h;
}

static void sdl_set_filtering(void *data, unsigned index, bool smooth)
{
   sdl_video_t *vid = (sdl_video_t*)data;
   vid->scaler.scaler_type = smooth ? SCALER_TYPE_BILINEAR : SCALER_TYPE_POINT;
}

static void sdl_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   switch (aspect_ratio_idx)
   {
      case ASPECT_RATIO_SQUARE:
         video_driver_set_viewport_square_pixel();
         break;

      case ASPECT_RATIO_CORE:
         video_driver_set_viewport_core();
         break;

      case ASPECT_RATIO_CONFIG:
         video_driver_set_viewport_config();
         break;

      default:
         break;
   }

   video_driver_set_aspect_ratio_value(aspectratio_lut[aspect_ratio_idx].value);
}

static void sdl_apply_state_changes(void *data)
{
   (void)data;
}

#ifdef HAVE_MENU
static void sdl_set_texture_frame(void *data, const void *frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
   enum scaler_pix_fmt format = rgb32 
      ? SCALER_FMT_ARGB8888 : SCALER_FMT_RGBA4444;
   sdl_video_t           *vid = (sdl_video_t*)data;

   video_frame_scale(
         &vid->menu.scaler,
         vid->menu.frame->pixels,
         frame,
         format,
         vid->menu.frame->w,
         vid->menu.frame->h,
         vid->menu.frame->pitch,
         width,
         height,
         width * (rgb32 ? sizeof(uint32_t) : sizeof(uint16_t))
         );

   SDL_SetAlpha(vid->menu.frame, SDL_SRCALPHA, 255.0 * alpha);
}

static void sdl_set_texture_enable(void *data, bool state, bool full_screen)
{
   sdl_video_t *vid = (sdl_video_t*)data;

   (void)full_screen;

   vid->menu.active = state;
}

static void sdl_show_mouse(void *data, bool state)
{
   (void)data;

   SDL_ShowCursor(state);
}
#endif

static void sdl_grab_mouse_toggle(void *data)
{
   const SDL_GrabMode mode = SDL_WM_GrabInput(SDL_GRAB_QUERY);

   (void)data;

   SDL_WM_GrabInput(mode == SDL_GRAB_ON ? SDL_GRAB_OFF : SDL_GRAB_ON);
}

static const video_poke_interface_t sdl_poke_interface = {
   NULL,
   NULL,
   NULL,
   sdl_set_filtering,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   sdl_set_aspect_ratio,
   sdl_apply_state_changes,
#ifdef HAVE_MENU
   sdl_set_texture_frame,
#endif
#ifdef HAVE_MENU
   sdl_set_texture_enable,
   NULL,
   sdl_show_mouse,
#else
   NULL,
   NULL,
   NULL,
#endif
   sdl_grab_mouse_toggle,
   NULL
};

static void sdl_get_poke_interface(void *data, const video_poke_interface_t **iface)
{
   (void)data;

   *iface = &sdl_poke_interface;
}

static bool sdl_gfx_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   (void)data;
   (void)type;
   (void)path;

   return false; 
}

static void sdl_gfx_set_rotation(void *data, unsigned rotation)
{
   (void)data;
   (void)rotation;
}

static bool sdl_gfx_read_viewport(void *data, uint8_t *buffer)
{
   (void)data;
   (void)buffer;

   return true;
}

video_driver_t video_sdl = {
   sdl_gfx_init,
   sdl_gfx_frame,
   sdl_gfx_set_nonblock_state,
   sdl_gfx_alive,
   sdl_gfx_focus,
   sdl_gfx_suppress_screensaver,
   sdl_gfx_has_windowed,
   sdl_gfx_set_shader,
   sdl_gfx_free,
   "sdl",
   NULL,
   sdl_gfx_set_rotation,
   sdl_gfx_viewport_info,
   sdl_gfx_read_viewport,
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   NULL,
#endif
   sdl_get_poke_interface
};
