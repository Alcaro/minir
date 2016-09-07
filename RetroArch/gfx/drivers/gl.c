/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
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

#ifdef _MSC_VER
#pragma comment(lib, "opengl32")
#endif

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <compat/strl.h>
#include <gfx/scaler/scaler.h>
#include <gfx/math/matrix_4x4.h>
#include <formats/image.h>
#include <retro_inline.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>
#include <libretro.h>

#include "gl_capabilities.h"
#include "gl_renderchains/render_chain_gl.h"

#include "../../configuration.h"
#include "../../driver.h"
#include "../../record/record_driver.h"
#include "../../performance_counters.h"

#include "../../retroarch.h"
#include "../../runloop.h"
#include "../../verbosity.h"
#include "../common/gl_common.h"

#ifdef HAVE_THREADS
#include "../video_thread_wrapper.h"
#endif

#include "../font_driver.h"
#include "../video_context_driver.h"
#include "../video_frame.h"

#ifdef HAVE_GLSL
#include "../drivers_shader/shader_glsl.h"
#endif

#ifdef GL_DEBUG
#include <lists/string_list.h>

#if defined(HAVE_OPENGLES2) || defined(HAVE_OPENGLES3) || defined(HAVE_OPENGLES_3_1) || defined(HAVE_OPENGLES_3_2)
#define HAVE_GL_DEBUG_ES
#endif
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#if defined(_WIN32) && !defined(_XBOX)
#include "../common/win32_common.h"
#endif

#include "../video_shader_driver.h"

#ifndef GL_SYNC_GPU_COMMANDS_COMPLETE
#define GL_SYNC_GPU_COMMANDS_COMPLETE     0x9117
#endif

#ifndef GL_SYNC_FLUSH_COMMANDS_BIT
#define GL_SYNC_FLUSH_COMMANDS_BIT        0x00000001
#endif

#ifdef IOS
/* There is no default frame buffer on iOS. */
void cocoagl_bind_game_view_fbo(void);
#define gl_bind_backbuffer() cocoagl_bind_game_view_fbo()
#else
#define gl_bind_backbuffer() glBindFramebuffer(RARCH_GL_FRAMEBUFFER, 0)
#endif

/* Used for the last pass when rendering to the back buffer. */
static const GLfloat vertexes_flipped[] = {
   0, 1,
   1, 1,
   0, 0,
   1, 0
};

/* Used when rendering to an FBO.
 * Texture coords have to be aligned 
 * with vertex coordinates. */
static const GLfloat vertexes[] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const GLfloat tex_coords[] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const GLfloat white_color[] = {
   1, 1, 1, 1,
   1, 1, 1, 1,
   1, 1, 1, 1,
   1, 1, 1, 1,
};

static bool gl_shared_context_use = false;

void context_bind_hw_render(bool enable)
{
   if (gl_shared_context_use)
      video_context_driver_bind_hw_render(&enable);
}


#ifdef HAVE_OVERLAY
static void gl_free_overlay(gl_t *gl)
{
   if (!gl)
      return;

   glDeleteTextures(gl->overlays, gl->overlay_tex);

   free(gl->overlay_tex);
   free(gl->overlay_vertex_coord);
   free(gl->overlay_tex_coord);
   free(gl->overlay_color_coord);
   gl->overlay_tex          = NULL;
   gl->overlay_vertex_coord = NULL;
   gl->overlay_tex_coord    = NULL;
   gl->overlay_color_coord  = NULL;
   gl->overlays             = 0;
}

static void gl_overlay_vertex_geom(void *data,
      unsigned image,
      float x, float y,
      float w, float h)
{
   GLfloat *vertex = NULL;
   gl_t *gl        = (gl_t*)data;

   if (!gl)
      return;

   if (image > gl->overlays)
   {
      RARCH_ERR("Invalid overlay id: %u\n", image);
      return;
   }

   vertex          = (GLfloat*)&gl->overlay_vertex_coord[image * 8];

   /* Flipped, so we preserve top-down semantics. */
   y               = 1.0f - y;
   h               = -h;

   vertex[0]       = x;
   vertex[1]       = y;
   vertex[2]       = x + w;
   vertex[3]       = y;
   vertex[4]       = x;
   vertex[5]       = y + h;
   vertex[6]       = x + w;
   vertex[7]       = y + h;
}

static void gl_overlay_tex_geom(void *data,
      unsigned image,
      GLfloat x, GLfloat y,
      GLfloat w, GLfloat h)
{
   GLfloat *tex = NULL;
   gl_t *gl     = (gl_t*)data;

   if (!gl)
      return;

   tex          = (GLfloat*)&gl->overlay_tex_coord[image * 8];

   tex[0]       = x;
   tex[1]       = y;
   tex[2]       = x + w;
   tex[3]       = y;
   tex[4]       = x;
   tex[5]       = y + h;
   tex[6]       = x + w;
   tex[7]       = y + h;
}

static void gl_render_overlay(gl_t *gl)
{
   video_shader_ctx_mvp_t mvp;
   video_shader_ctx_coords_t coords;
   video_shader_ctx_info_t shader_info;
   unsigned i, width, height;

   if (!gl || !gl->overlay_enable)
      return;

   video_driver_get_size(&width, &height);

   glEnable(GL_BLEND);

   if (gl->overlay_full_screen)
      glViewport(0, 0, width, height);

   /* Ensure that we reset the attrib array. */
   shader_info.data       = gl;
   shader_info.idx        = VIDEO_SHADER_STOCK_BLEND;
   shader_info.set_active = true;

   video_shader_driver_use(&shader_info);

   gl->coords.vertex    = gl->overlay_vertex_coord;
   gl->coords.tex_coord = gl->overlay_tex_coord;
   gl->coords.color     = gl->overlay_color_coord;
   gl->coords.vertices  = 4 * gl->overlays;

   coords.handle_data   = NULL;
   coords.data          = &gl->coords;

   video_shader_driver_set_coords(&coords);

   mvp.data             = gl;
   mvp.matrix           = &gl->mvp_no_rot;

   video_shader_driver_set_mvp(&mvp);

   for (i = 0; i < gl->overlays; i++)
   {
      glBindTexture(GL_TEXTURE_2D, gl->overlay_tex[i]);
      glDrawArrays(GL_TRIANGLE_STRIP, 4 * i, 4);
   }

   glDisable(GL_BLEND);
   gl->coords.vertex    = gl->vertex_ptr;
   gl->coords.tex_coord = gl->tex_info.coord;
   gl->coords.color     = gl->white_color_ptr;
   gl->coords.vertices  = 4;
   if (gl->overlay_full_screen)
      glViewport(gl->vp.x, gl->vp.y, gl->vp.width, gl->vp.height);
}
#endif

#define set_texture_coords(coords, xamt, yamt) \
   coords[2] = xamt; \
   coords[6] = xamt; \
   coords[5] = yamt; \
   coords[7] = yamt

static void gl_set_projection(gl_t *gl,
      struct video_ortho *ortho, bool allow_rotate)
{
   math_matrix_4x4 rot;

   /* Calculate projection. */
   matrix_4x4_ortho(&gl->mvp_no_rot, ortho->left, ortho->right,
         ortho->bottom, ortho->top, ortho->znear, ortho->zfar);

   if (!allow_rotate)
   {
      gl->mvp = gl->mvp_no_rot;
      return;
   }

   matrix_4x4_rotate_z(&rot, M_PI * gl->rotation / 180.0f);
   matrix_4x4_multiply(&gl->mvp, &rot, &gl->mvp_no_rot);
}

void gl_set_viewport(void *data, unsigned viewport_width,
      unsigned viewport_height, bool force_full, bool allow_rotate)
{
   gfx_ctx_aspect_t aspect_data;
   unsigned width, height;
   int x                  = 0;
   int y                  = 0;
   float device_aspect    = (float)viewport_width / viewport_height;
   struct video_ortho ortho = {0, 1, 0, 1, -1, 1};
   settings_t *settings   = config_get_ptr();
   gl_t           *gl     = (gl_t*)data;

   video_driver_get_size(&width, &height);

   aspect_data.aspect   = &device_aspect;
   aspect_data.width    = viewport_width;
   aspect_data.height   = viewport_height;

   video_context_driver_translate_aspect(&aspect_data);

   if (settings->video.scale_integer && !force_full)
   {
      video_viewport_get_scaled_integer(&gl->vp,
            viewport_width, viewport_height,
            video_driver_get_aspect_ratio(), gl->keep_aspect);
      viewport_width  = gl->vp.width;
      viewport_height = gl->vp.height;
   }
   else if (gl->keep_aspect && !force_full)
   {
      float desired_aspect = video_driver_get_aspect_ratio();

#if defined(HAVE_MENU)
      if (settings->video.aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
      {
         const struct video_viewport *custom = video_viewport_get_custom();

         /* GL has bottom-left origin viewport. */
         x      = custom->x;
         y      = height - custom->y - custom->height;
         viewport_width  = custom->width;
         viewport_height = custom->height;
      }
      else
#endif
      {
         float delta;

         if (fabsf(device_aspect - desired_aspect) < 0.0001f)
         {
            /* If the aspect ratios of screen and desired aspect 
             * ratio are sufficiently equal (floating point stuff), 
             * assume they are actually equal.
             */
         }
         else if (device_aspect > desired_aspect)
         {
            delta = (desired_aspect / device_aspect - 1.0f) / 2.0f + 0.5f;
            x     = (int)roundf(viewport_width * (0.5f - delta));
            viewport_width = (unsigned)roundf(2.0f * viewport_width * delta);
         }
         else
         {
            delta  = (device_aspect / desired_aspect - 1.0f) / 2.0f + 0.5f;
            y      = (int)roundf(viewport_height * (0.5f - delta));
            viewport_height = (unsigned)roundf(2.0f * viewport_height * delta);
         }
      }

      gl->vp.x      = x;
      gl->vp.y      = y;
      gl->vp.width  = viewport_width;
      gl->vp.height = viewport_height;
   }
   else
   {
      gl->vp.x      = gl->vp.y = 0;
      gl->vp.width  = viewport_width;
      gl->vp.height = viewport_height;
   }

#if defined(RARCH_MOBILE)
   /* In portrait mode, we want viewport to gravitate to top of screen. */
   if (device_aspect < 1.0f)
      gl->vp.y *= 2;
#endif

   glViewport(gl->vp.x, gl->vp.y, gl->vp.width, gl->vp.height);
   gl_set_projection(gl, &ortho, allow_rotate);

   /* Set last backbuffer viewport. */
   if (!force_full)
   {
      gl->vp_out_width  = viewport_width;
      gl->vp_out_height = viewport_height;
   }

#if 0
   RARCH_LOG("Setting viewport @ %ux%u\n", viewport_width, viewport_height);
#endif
}


/* Shaders */

static bool gl_shader_init(gl_t *gl)
{
   video_shader_ctx_init_t init_data;
   enum rarch_shader_type type;
   settings_t *settings            = config_get_ptr();
   const char *shader_path         = (settings->video.shader_enable 
         && *settings->path.shader) ? settings->path.shader : NULL;

   if (!gl)
   {
      RARCH_ERR("Invalid GL instance passed.\n");
      return false;
   }

   type = video_shader_parse_type(shader_path,
         gl_query_core_context_in_use() 
         ? RARCH_SHADER_GLSL : DEFAULT_SHADER_TYPE);

   switch (type)
   {
#ifdef HAVE_CG
      case RARCH_SHADER_CG:
         if (gl_query_core_context_in_use())
            shader_path = NULL;
         break;
#endif

#ifdef HAVE_GLSL
      case RARCH_SHADER_GLSL:
         break;
#endif

      default:
         RARCH_ERR("[GL]: Not loading any shader, or couldn't find valid shader backend. Continuing without shaders.\n");
         return true;
   }

   init_data.gl.core_context_enabled = gl_query_core_context_in_use();
   init_data.shader_type             = type;
   init_data.shader                  = NULL;
   init_data.data                    = gl;
   init_data.path                    = shader_path;

   if (video_shader_driver_init(&init_data))
      return true;

   RARCH_ERR("[GL]: Failed to initialize shader, falling back to stock.\n");

   init_data.shader = NULL;
   init_data.path   = NULL;

   return video_shader_driver_init(&init_data);
}

#ifndef NO_GL_FF_VERTEX
static void gl_disable_client_arrays(void)
{
   if (gl_query_core_context_in_use())
      return;

   glClientActiveTexture(GL_TEXTURE1);
   glDisableClientState(GL_TEXTURE_COORD_ARRAY);
   glClientActiveTexture(GL_TEXTURE0);
   glDisableClientState(GL_VERTEX_ARRAY);
   glDisableClientState(GL_COLOR_ARRAY);
   glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}
#endif


GLenum min_filter_to_mag(GLenum type)
{
   switch (type)
   {
      case GL_LINEAR_MIPMAP_LINEAR:
         return GL_LINEAR;
      case GL_NEAREST_MIPMAP_NEAREST:
         return GL_NEAREST;
      default:
         break;
   }

   return type;
}

#ifdef HAVE_FBO
static uintptr_t gl_get_current_framebuffer(void *data)
{
   gl_t *gl = (gl_t*)data;
   return gl->hw_render_fbo[(gl->tex_index + 1) % gl->textures];
}
#endif

static void gl_set_rotation(void *data, unsigned rotation)
{
   gl_t               *gl = (gl_t*)data;
   struct video_ortho ortho = {0, 1, 0, 1, -1, 1};

   if (!gl)
      return;

   gl->rotation = 90 * rotation;
   gl_set_projection(gl, &ortho, true);
}

static void gl_set_video_mode(void *data, unsigned width, unsigned height,
      bool fullscreen)
{
   gfx_ctx_mode_t mode;

   mode.width      = width;
   mode.height     = height;
   mode.fullscreen = fullscreen;

   video_context_driver_set_video_mode(&mode);
}


static void gl_update_input_size(gl_t *gl, unsigned width,
      unsigned height, unsigned pitch, bool clear)
{
   GLfloat xamt, yamt;
   bool set_coords = false;


   if ((width != gl->last_width[gl->tex_index] || 
            height != gl->last_height[gl->tex_index]) && gl->empty_buf)
   {
      /* Resolution change. Need to clear out texture. */

      gl->last_width[gl->tex_index]  = width;
      gl->last_height[gl->tex_index] = height;

      if (clear)
      {
         glPixelStorei(GL_UNPACK_ALIGNMENT,
               video_pixel_get_alignment(width * sizeof(uint32_t)));
#if defined(HAVE_PSGL)
         glBufferSubData(GL_TEXTURE_REFERENCE_BUFFER_SCE,
               gl->tex_w * gl->tex_h * gl->tex_index * gl->base_size,
               gl->tex_w * gl->tex_h * gl->base_size,
               gl->empty_buf);
#else
         glTexSubImage2D(GL_TEXTURE_2D,
               0, 0, 0, gl->tex_w, gl->tex_h, gl->texture_type,
               gl->texture_fmt, gl->empty_buf);
#endif
      }

      set_coords = true;
   }
   else if ((width != 
            gl->last_width[(gl->tex_index + gl->textures - 1) % gl->textures]) ||
         (height != 
          gl->last_height[(gl->tex_index + gl->textures - 1) % gl->textures]))
   {
      /* We might have used different texture coordinates 
       * last frame. Edge case if resolution changes very rapidly. */
      set_coords = true;
   }

   if (!set_coords)
      return;

   xamt = (GLfloat)width  / gl->tex_w;
   yamt = (GLfloat)height / gl->tex_h;
   set_texture_coords(gl->tex_info.coord, xamt, yamt);
}

static void gl_init_textures_data(gl_t *gl)
{
   unsigned i;

   for (i = 0; i < gl->textures; i++)
   {
      gl->last_width[i]  = gl->tex_w;
      gl->last_height[i] = gl->tex_h;
   }

   for (i = 0; i < gl->textures; i++)
   {
      gl->prev_info[i].tex           = gl->texture[0];
      gl->prev_info[i].input_size[0] = gl->tex_w;
      gl->prev_info[i].tex_size[0]   = gl->tex_w;
      gl->prev_info[i].input_size[1] = gl->tex_h;
      gl->prev_info[i].tex_size[1]   = gl->tex_h;
      memcpy(gl->prev_info[i].coord, tex_coords, sizeof(tex_coords)); 
   }
}

static void gl_init_textures_reference(gl_t *gl, unsigned i,
      GLenum internal_fmt, GLenum texture_fmt, GLenum texture_type)
{
   glBindTexture(GL_TEXTURE_2D, gl->texture[i]);

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl->wrap_mode);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl->wrap_mode);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl->tex_mag_filter);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl->tex_min_filter);

#ifdef HAVE_PSGL
   glTextureReferenceSCE(GL_TEXTURE_2D, 1,
         gl->tex_w, gl->tex_h, 0, 
         internal_fmt,
         gl->tex_w * gl->base_size,
         gl->tex_w * gl->tex_h * i * gl->base_size);
#else
   if (gl->egl_images)
      return;

   glTexImage2D(GL_TEXTURE_2D,
         0, internal_fmt, gl->tex_w, gl->tex_h, 0, texture_type,
         texture_fmt, gl->empty_buf ? gl->empty_buf : NULL);
#endif
}

static void gl_init_textures(gl_t *gl, const video_info_t *video)
{
   unsigned i;
   GLenum internal_fmt, texture_type = 0, texture_fmt = 0;

   (void)texture_type;
   (void)texture_fmt;

#if defined(HAVE_EGL) && defined(HAVE_OPENGLES)
   /* Use regular textures if we use HW render. */
   gl->egl_images = !gl->hw_render_use && gl_check_capability(GL_CAPS_EGLIMAGE) &&
      video_context_driver_init_image_buffer((void*)video);
#endif

#ifdef HAVE_PSGL
   if (!gl->pbo)
      glGenBuffers(1, &gl->pbo);

   glBindBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, gl->pbo);
   glBufferData(GL_TEXTURE_REFERENCE_BUFFER_SCE,
         gl->tex_w * gl->tex_h * gl->base_size * gl->textures, 
         NULL, GL_STREAM_DRAW);
#endif

   internal_fmt = gl->internal_fmt;
#ifndef HAVE_PSGL
   texture_type = gl->texture_type;
   texture_fmt  = gl->texture_fmt;
#endif

#ifdef HAVE_OPENGLES2
   /* GLES is picky about which format we use here.
    * Without extensions, we can *only* render to 16-bit FBOs. */

   if (gl->hw_render_use && gl->base_size == sizeof(uint32_t))
   {
      if (gl_check_capability(GL_CAPS_ARGB8))
      {
         internal_fmt = GL_RGBA;
         texture_type = GL_RGBA;
         texture_fmt  = GL_UNSIGNED_BYTE;
      }
      else
      {
         RARCH_WARN("[GL]: 32-bit FBO not supported. Falling back to 16-bit.\n");
         internal_fmt = GL_RGB;
         texture_type = GL_RGB;
         texture_fmt  = GL_UNSIGNED_SHORT_5_6_5;
      }
   }
#endif

   glGenTextures(gl->textures, gl->texture);

   for (i = 0; i < gl->textures; i++)
      gl_init_textures_reference(gl, i, internal_fmt,
            texture_fmt, texture_type);

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
}

static INLINE void gl_copy_frame(gl_t *gl, const void *frame,
      unsigned width, unsigned height, unsigned pitch)
{
   static struct retro_perf_counter copy_frame = {0};

   performance_counter_init(&copy_frame, "copy_frame");
   performance_counter_start(&copy_frame);

#if defined(HAVE_PSGL)
   {
      unsigned h;
      size_t buffer_addr        = gl->tex_w * gl->tex_h * 
         gl->tex_index * gl->base_size;
      size_t buffer_stride      = gl->tex_w * gl->base_size;
      const uint8_t *frame_copy = frame;
      size_t frame_copy_size    = width * gl->base_size;

      uint8_t *buffer = (uint8_t*)glMapBuffer(
            GL_TEXTURE_REFERENCE_BUFFER_SCE, GL_READ_WRITE) + buffer_addr;
      for (h = 0; h < height; h++, buffer += buffer_stride, frame_copy += pitch)
         memcpy(buffer, frame_copy, frame_copy_size);

      glUnmapBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE);
   }
#elif defined(HAVE_OPENGLES)
#if defined(HAVE_EGL)
   if (gl->egl_images)
   {
      gfx_ctx_image_t img_info;
      bool new_egl    = false;
      EGLImageKHR img = 0;

      img_info.frame  = frame;
      img_info.width  = width;
      img_info.height = height;
      img_info.pitch  = pitch;
      img_info.index  = gl->tex_index;
      img_info.rgb32  = (gl->base_size == 4);
      img_info.handle = &img;

      new_egl = video_context_driver_write_to_image_buffer(&img_info);

      if (img == EGL_NO_IMAGE_KHR)
      {
         RARCH_ERR("[GL]: Failed to create EGL image.\n");
         return;
      }

      if (new_egl)
         glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)img);
   }
   else
#endif
   {
      bool use_rgba = video_driver_supports_rgba();

      glPixelStorei(GL_UNPACK_ALIGNMENT, 
            video_pixel_get_alignment(width * gl->base_size));

      /* Fallback for GLES devices without GL_BGRA_EXT. */
      if (gl->base_size == 4 && use_rgba)
      {
         video_frame_convert_argb8888_to_abgr8888(
               &gl->scaler,
               gl->conv_buffer,
               frame, width, height, pitch);
         glTexSubImage2D(GL_TEXTURE_2D,
               0, 0, 0, width, height, gl->texture_type,
               gl->texture_fmt, gl->conv_buffer);
      }
      else if (gl->support_unpack_row_length)
      {
         glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch / gl->base_size);
         glTexSubImage2D(GL_TEXTURE_2D,
               0, 0, 0, width, height, gl->texture_type,
               gl->texture_fmt, frame);

         glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
      }
      else
      {
         /* No GL_UNPACK_ROW_LENGTH. */

         const GLvoid *data_buf = frame;
         unsigned pitch_width   = pitch / gl->base_size;

         if (width != pitch_width)
         {
            /* Slow path - conv_buffer is preallocated 
             * just in case we hit this path. */

            unsigned h;
            const unsigned line_bytes = width * gl->base_size;
            uint8_t *dst              = (uint8_t*)gl->conv_buffer;
            const uint8_t *src        = (const uint8_t*)frame;

            for (h = 0; h < height; h++, src += pitch, dst += line_bytes)
               memcpy(dst, src, line_bytes);

            data_buf                  = gl->conv_buffer;
         }

         glTexSubImage2D(GL_TEXTURE_2D,
               0, 0, 0, width, height, gl->texture_type,
               gl->texture_fmt, data_buf);         
      }
   }
#else
   {
      const GLvoid *data_buf = frame;
      glPixelStorei(GL_UNPACK_ALIGNMENT, video_pixel_get_alignment(pitch));

      if (gl->base_size == 2 && !gl->have_es2_compat)
      {
         /* Convert to 32-bit textures on desktop GL.
          *
          * It is *much* faster (order of magnitude on my setup)
          * to use a custom SIMD-optimized conversion routine 
          * than letting GL do it. */
         video_frame_convert_rgb16_to_rgb32(
               &gl->scaler,
               gl->conv_buffer,
               frame,
               width,
               height,
               pitch);
         data_buf = gl->conv_buffer;
      }
      else
         glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch / gl->base_size);

      glTexSubImage2D(GL_TEXTURE_2D,
            0, 0, 0, width, height, gl->texture_type,
            gl->texture_fmt, data_buf);

      glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
   }
#endif
   performance_counter_stop(&copy_frame);
}

static INLINE void gl_set_shader_viewport(gl_t *gl, unsigned idx)
{
   unsigned width, height;
   video_shader_ctx_info_t shader_info;

   video_driver_get_size(&width, &height);

   shader_info.data       = gl;
   shader_info.idx        = idx;
   shader_info.set_active = true;

   video_shader_driver_use(&shader_info);
   gl_set_viewport(gl, width, height, false, true);
}

void gl_load_texture_data(
      uint32_t id_data,
      enum gfx_wrap_type wrap_type,
      enum texture_filter_type filter_type,
      unsigned alignment,
      unsigned width, unsigned height,
      const void *frame, unsigned base_size)
{
   GLint mag_filter, min_filter;
   bool want_mipmap = false;
   bool use_rgba    = video_driver_supports_rgba();
   bool rgb32       = (base_size == (sizeof(uint32_t)));
   GLenum wrap      = gl_wrap_type_to_enum(wrap_type);
   GLuint id        = (GLuint)id_data;

   glBindTexture(GL_TEXTURE_2D, id);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
    
   if (!gl_check_capability(GL_CAPS_MIPMAP))
   {
      /* Assume no mipmapping support. */
      switch (filter_type)
      {
         case TEXTURE_FILTER_MIPMAP_LINEAR:
            filter_type = TEXTURE_FILTER_LINEAR;
            break;
         case TEXTURE_FILTER_MIPMAP_NEAREST:
            filter_type = TEXTURE_FILTER_NEAREST;
            break;
         default:
            break;
      }
   }

   switch (filter_type)
   {
      case TEXTURE_FILTER_MIPMAP_LINEAR:
         min_filter = GL_LINEAR_MIPMAP_NEAREST;
         mag_filter = GL_LINEAR;
         want_mipmap = true;
         break;
      case TEXTURE_FILTER_MIPMAP_NEAREST:
         min_filter = GL_NEAREST_MIPMAP_NEAREST;
         mag_filter = GL_NEAREST;
         want_mipmap = true;
         break;
      case TEXTURE_FILTER_NEAREST:
         min_filter = GL_NEAREST;
         mag_filter = GL_NEAREST;
         break;
      case TEXTURE_FILTER_LINEAR:
      default:
         min_filter = GL_LINEAR;
         mag_filter = GL_LINEAR;
         break;
   }

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);

   glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
   glTexImage2D(GL_TEXTURE_2D,
         0,
         (use_rgba || !rgb32) ? GL_RGBA : RARCH_GL_INTERNAL_FORMAT32,
         width, height, 0,
         (use_rgba || !rgb32) ? GL_RGBA : RARCH_GL_TEXTURE_TYPE32,
         (rgb32) ? RARCH_GL_FORMAT32 : GL_UNSIGNED_SHORT_4_4_4_4, frame);

   if (want_mipmap && gl_check_capability(GL_CAPS_MIPMAP))
      glGenerateMipmap(GL_TEXTURE_2D);
}


#if defined(HAVE_MENU)
static void gl_set_texture_frame(void *data,
      const void *frame, bool rgb32, unsigned width, unsigned height,
      float alpha)
{
   enum texture_filter_type menu_filter;
   settings_t *settings            = config_get_ptr();
   unsigned base_size              = rgb32 ? sizeof(uint32_t) : sizeof(uint16_t);
   gl_t *gl                        = (gl_t*)data;
   if (!gl)
      return;

   context_bind_hw_render(false);

   menu_filter = settings->menu.linear_filter ? TEXTURE_FILTER_LINEAR : TEXTURE_FILTER_NEAREST;

   if (!gl->menu_texture)
      glGenTextures(1, &gl->menu_texture);


   gl_load_texture_data(gl->menu_texture,
         RARCH_WRAP_EDGE, menu_filter,
         video_pixel_get_alignment(width * base_size),
         width, height, frame,
         base_size);

   gl->menu_texture_alpha = alpha;
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

   context_bind_hw_render(true);
}

static void gl_set_texture_enable(void *data, bool state, bool full_screen)
{
   gl_t *gl                     = (gl_t*)data;

   if (!gl)
      return;

   gl->menu_texture_enable      = state;
   gl->menu_texture_full_screen = full_screen;
}

static void gl_set_osd_msg(void *data, const char *msg,
      const struct font_params *params, void *font)
{
   font_driver_render_msg(font, msg, params);
}

static void gl_show_mouse(void *data, bool state)
{
   video_context_driver_show_mouse(&state);
}

static struct video_shader *gl_get_current_shader(void *data)
{
   video_shader_ctx_t shader_info;

   video_shader_driver_direct_get_current_shader(&shader_info);

   return shader_info.data;
}

#if defined(HAVE_GL_ASYNC_READBACK)
static void gl_pbo_async_readback(gl_t *gl)
{
   static struct retro_perf_counter async_readback = {0};

   glBindBuffer(GL_PIXEL_PACK_BUFFER,
         gl->pbo_readback[gl->pbo_readback_index++]);
   gl->pbo_readback_index &= 3;

   /* 4 frames back, we can readback. */
   gl->pbo_readback_valid[gl->pbo_readback_index] = true;

   glPixelStorei(GL_PACK_ROW_LENGTH, 0);
   glPixelStorei(GL_PACK_ALIGNMENT,
         video_pixel_get_alignment(gl->vp.width * sizeof(uint32_t)));

   /* Read asynchronously into PBO buffer. */
   performance_counter_init(&async_readback, "async_readback");
   performance_counter_start(&async_readback);
   glReadBuffer(GL_BACK);
#ifdef HAVE_OPENGLES3
   glReadPixels(gl->vp.x, gl->vp.y,
         gl->vp.width, gl->vp.height,
         GL_RGBA, GL_UNSIGNED_BYTE, NULL);
#else
   glReadPixels(gl->vp.x, gl->vp.y,
         gl->vp.width, gl->vp.height,
         GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
#endif
   performance_counter_stop(&async_readback);

   glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}
#endif

static INLINE void gl_draw_texture(gl_t *gl)
{
   video_shader_ctx_mvp_t mvp;
   video_shader_ctx_coords_t coords;
   video_shader_ctx_info_t shader_info;
   unsigned width, height;
   GLfloat color[16];

   color[ 0] = 1.0f;
   color[ 1] = 1.0f;
   color[ 2] = 1.0f;
   color[ 3] = gl->menu_texture_alpha;
   color[ 4] = 1.0f;
   color[ 5] = 1.0f;
   color[ 6] = 1.0f;
   color[ 7] = gl->menu_texture_alpha;
   color[ 8] = 1.0f;
   color[ 9] = 1.0f;
   color[10] = 1.0f;
   color[11] = gl->menu_texture_alpha;
   color[12] = 1.0f;
   color[13] = 1.0f;
   color[14] = 1.0f;
   color[15] = gl->menu_texture_alpha;

   if (!gl->menu_texture)
      return;

   video_driver_get_size(&width, &height);

   gl->coords.vertex    = vertexes_flipped;
   gl->coords.tex_coord = tex_coords;
   gl->coords.color     = color;
   glBindTexture(GL_TEXTURE_2D, gl->menu_texture);

   shader_info.data       = gl;
   shader_info.idx        = VIDEO_SHADER_STOCK_BLEND;
   shader_info.set_active = true;

   video_shader_driver_use(&shader_info);

   gl->coords.vertices  = 4;

   coords.handle_data   = NULL;
   coords.data          = &gl->coords;

   video_shader_driver_set_coords(&coords);

   mvp.data             = gl;
   mvp.matrix           = &gl->mvp_no_rot;

   video_shader_driver_set_mvp(&mvp);

   glEnable(GL_BLEND);

   if (gl->menu_texture_full_screen)
   {
      glViewport(0, 0, width, height);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      glViewport(gl->vp.x, gl->vp.y, gl->vp.width, gl->vp.height);
   }
   else
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

   glDisable(GL_BLEND);

   gl->coords.vertex    = gl->vertex_ptr;
   gl->coords.tex_coord = gl->tex_info.coord;
   gl->coords.color     = gl->white_color_ptr;
}
#endif


static bool gl_frame(void *data, const void *frame,
      unsigned frame_width, unsigned frame_height,
      uint64_t frame_count,
      unsigned pitch, const char *msg)
{
   video_shader_ctx_mvp_t mvp;
   video_shader_ctx_coords_t coords;
   video_shader_ctx_params_t params;
   unsigned width, height;
   struct video_tex_info feedback_info;
   video_shader_ctx_info_t shader_info;
   static struct retro_perf_counter frame_run = {0};
   gl_t                            *gl = (gl_t*)data;
   settings_t                *settings = config_get_ptr();

   performance_counter_init(&frame_run, "frame_run");
   performance_counter_start(&frame_run);

   if (!gl)
      return false;

   video_driver_get_size(&width, &height);

   context_bind_hw_render(false);

#ifndef HAVE_OPENGLES
   if (gl_query_core_context_in_use())
      glBindVertexArray(gl->vao);
#endif

   shader_info.data       = gl;
   shader_info.idx        = 1;
   shader_info.set_active = true;

   video_shader_driver_use(&shader_info);

#ifdef IOS
   /* Apparently the viewport is lost each frame, thanks Apple. */
   gl_set_viewport(gl, width, height, false, true);
#endif

#ifdef HAVE_FBO
   /* Render to texture in first pass. */
   if (gl->fbo_inited)
   {
      gl_renderchain_recompute_pass_sizes(gl, frame_width, frame_height,
            gl->vp_out_width, gl->vp_out_height);
      gl_renderchain_start_render(gl);
   }
#endif

   if (gl->should_resize)
   {
      gfx_ctx_mode_t mode;

      gl->should_resize = false;

      mode.width        = width;
      mode.height       = height;

      video_context_driver_set_resize(&mode);

#ifdef HAVE_FBO
      if (gl->fbo_inited)
      {
         gl_check_fbo_dimensions(gl);

         /* Go back to what we're supposed to do, 
          * render to FBO #0. */
         gl_renderchain_start_render(gl);
      }
      else
#endif
         gl_set_viewport(gl, width, height, false, true);
   }

   if (frame)
      gl->tex_index = ((gl->tex_index + 1) % gl->textures);

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

   /* Can be NULL for frame dupe / NULL render. */
   if (frame) 
   {
#ifdef HAVE_FBO
      if (!gl->hw_render_fbo_init)
#endif
      {
         gl_update_input_size(gl, frame_width, frame_height, pitch, true);
         gl_copy_frame(gl, frame, frame_width, frame_height, pitch);
      }

      /* No point regenerating mipmaps 
       * if there are no new frames. */
      if (gl->tex_mipmap && gl_check_capability(GL_CAPS_MIPMAP))
         glGenerateMipmap(GL_TEXTURE_2D);
   }

   /* Have to reset rendering state which libretro core 
    * could easily have overridden. */
#ifdef HAVE_FBO
   if (gl->hw_render_fbo_init)
   {
      gl_update_input_size(gl, frame_width, frame_height, pitch, false);
      if (!gl->fbo_inited)
      {
         gl_bind_backbuffer();
         gl_set_viewport(gl, width, height, false, true);
      }

#ifndef HAVE_OPENGLES
      if (!gl_query_core_context_in_use())
         glEnable(GL_TEXTURE_2D);
#endif
      glDisable(GL_DEPTH_TEST);
      glDisable(GL_STENCIL_TEST);
      glDisable(GL_CULL_FACE);
      glDisable(GL_DITHER);
      glDisable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glBlendEquation(GL_FUNC_ADD);
      glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
   }
#endif

   gl->tex_info.tex           = gl->texture[gl->tex_index];
   gl->tex_info.input_size[0] = frame_width;
   gl->tex_info.input_size[1] = frame_height;
   gl->tex_info.tex_size[0]   = gl->tex_w;
   gl->tex_info.tex_size[1]   = gl->tex_h;

   feedback_info              = gl->tex_info;

#ifdef HAVE_FBO
   if (gl->fbo_feedback_enable)
   {
      const struct video_fbo_rect *rect = &gl->fbo_rect[gl->fbo_feedback_pass];
      GLfloat xamt                    = (GLfloat)rect->img_width / rect->width;
      GLfloat yamt                    = (GLfloat)rect->img_height / rect->height;

      feedback_info.tex               = gl->fbo_feedback_texture;
      feedback_info.input_size[0]     = rect->img_width;
      feedback_info.input_size[1]     = rect->img_height;
      feedback_info.tex_size[0]       = rect->width;
      feedback_info.tex_size[1]       = rect->height;

      set_texture_coords(feedback_info.coord, xamt, yamt);
   }
#endif

   glClear(GL_COLOR_BUFFER_BIT);

   params.data          = gl;
   params.width         = frame_width;
   params.height        = frame_height;
   params.tex_width     = gl->tex_w;
   params.tex_height    = gl->tex_h;
   params.out_width     = gl->vp.width;
   params.out_height    = gl->vp.height;
   params.frame_counter = (unsigned int)frame_count;
   params.info          = &gl->tex_info;
   params.prev_info     = gl->prev_info;
   params.feedback_info = &feedback_info;
   params.fbo_info      = NULL;
   params.fbo_info_cnt  = 0;

   video_shader_driver_set_parameters(&params);

   gl->coords.vertices = 4;
   coords.handle_data   = NULL;
   coords.data          = &gl->coords;

   video_shader_driver_set_coords(&coords);

   mvp.data             = gl;
   mvp.matrix           = &gl->mvp;

   video_shader_driver_set_mvp(&mvp);

   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

#ifdef HAVE_FBO
   if (gl->fbo_inited)
      gl_renderchain_render(gl, frame_count, &gl->tex_info, &feedback_info);
#endif

   /* Set prev textures. */
   gl_renderchain_bind_prev_texture(gl, &gl->tex_info);

#if defined(HAVE_MENU)
   if (gl->menu_texture_enable)
   {
      menu_driver_ctl(RARCH_MENU_CTL_FRAME, NULL);

      if (gl->menu_texture_enable)
         gl_draw_texture(gl);
   }
#endif

   if (font_driver_has_render_msg() && msg)
      font_driver_render_msg(NULL, msg, NULL);

#ifdef HAVE_OVERLAY
   gl_render_overlay(gl);
#endif

   video_context_driver_update_window_title();

   performance_counter_stop(&frame_run);

#ifdef HAVE_FBO
   /* Reset state which could easily mess up libretro core. */
   if (gl->hw_render_fbo_init)
   {
      shader_info.data       = gl;
      shader_info.idx        = 0;
      shader_info.set_active = true;

      video_shader_driver_use(&shader_info);

      glBindTexture(GL_TEXTURE_2D, 0);
#ifndef NO_GL_FF_VERTEX
      gl_disable_client_arrays();
#endif
   }
#endif

#ifndef NO_GL_READ_PIXELS
   /* Screenshots. */
   if (gl->readback_buffer_screenshot)
   {
      glPixelStorei(GL_PACK_ALIGNMENT, 4);
#ifndef HAVE_OPENGLES
      glPixelStorei(GL_PACK_ROW_LENGTH, 0);
      glReadBuffer(GL_BACK);
#endif
      glReadPixels(gl->vp.x, gl->vp.y,
            gl->vp.width, gl->vp.height,
            GL_RGBA, GL_UNSIGNED_BYTE, gl->readback_buffer_screenshot);
   }
#ifdef HAVE_GL_ASYNC_READBACK
#ifdef HAVE_MENU
   /* Don't readback if we're in menu mode. */
   else if (gl->pbo_readback_enable && !gl->menu_texture_enable)
      gl_pbo_async_readback(gl);
#endif
#endif
#endif

   /* Disable BFI during fast forward, slow-motion,
    * and pause to prevent flicker. */
   if (
         settings->video.black_frame_insertion
         && !input_driver_is_nonblock_state()
         && !runloop_ctl(RUNLOOP_CTL_IS_SLOWMOTION, NULL)
         && !runloop_ctl(RUNLOOP_CTL_IS_PAUSED, NULL))
   {
      video_context_driver_swap_buffers();
      glClear(GL_COLOR_BUFFER_BIT);
   }

   video_context_driver_swap_buffers();

#ifdef HAVE_GL_SYNC
   if (settings->video.hard_sync && gl->have_sync)
   {
      static struct retro_perf_counter gl_fence = {0};

      performance_counter_init(&gl_fence, "gl_fence");
      performance_counter_start(&gl_fence);
      glClear(GL_COLOR_BUFFER_BIT);
      gl->fences[gl->fence_count++] = 
         glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

      while (gl->fence_count > settings->video.hard_sync_frames)
      {
         glClientWaitSync(gl->fences[0],
               GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000);
         glDeleteSync(gl->fences[0]);

         gl->fence_count--;
         memmove(gl->fences, gl->fences + 1,
               gl->fence_count * sizeof(GLsync));
      }

      performance_counter_stop(&gl_fence);
   }
#endif

#ifndef HAVE_OPENGLES
   if (gl_query_core_context_in_use())
      glBindVertexArray(0);
#endif

   context_bind_hw_render(true);

   return true;
}


static void gl_destroy_resources(gl_t *gl)
{
   if (gl)
   {
      if (gl->empty_buf)
         free(gl->empty_buf);
      if (gl->conv_buffer)
         free(gl->conv_buffer);
      free(gl);
   }

   gl_shared_context_use = false;
   gl_query_core_context_unset();
}

static void gl_free(void *data)
{
   gl_t *gl = (gl_t*)data;
   if (!gl)
      return;

   context_bind_hw_render(false);

#ifdef HAVE_GL_SYNC
   if (gl->have_sync)
   {
      unsigned i;

      for (i = 0; i < gl->fence_count; i++)
      {
         glClientWaitSync(gl->fences[i],
               GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000);
         glDeleteSync(gl->fences[i]);
      }
      gl->fence_count = 0;
   }
#endif

   if (font_driver_has_render_msg())
      font_driver_free(NULL);
   video_shader_driver_deinit();

#ifndef NO_GL_FF_VERTEX
   gl_disable_client_arrays();
#endif

   glDeleteTextures(gl->textures, gl->texture);

#if defined(HAVE_MENU)
   if (gl->menu_texture)
      glDeleteTextures(1, &gl->menu_texture);
#endif

#ifdef HAVE_OVERLAY
   gl_free_overlay(gl);
#endif

#if defined(HAVE_PSGL)
   glBindBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, 0);
   glDeleteBuffers(1, &gl->pbo);
#endif

   scaler_ctx_gen_reset(&gl->scaler);

#ifdef HAVE_GL_ASYNC_READBACK
   if (gl->pbo_readback_enable)
   {
      glDeleteBuffers(4, gl->pbo_readback);
      scaler_ctx_gen_reset(&gl->pbo_readback_scaler);
   }
#endif

#ifdef HAVE_FBO
   gl_renderchain_free(gl);
#endif

#ifndef HAVE_OPENGLES
   if (gl_query_core_context_in_use())
   {
      glBindVertexArray(0);
      glDeleteVertexArrays(1, &gl->vao);
   }
#endif

   video_context_driver_free();

   gl_destroy_resources(gl);
}

static void gl_set_nonblock_state(void *data, bool state)
{
   unsigned interval           = 0;
   gl_t             *gl        = (gl_t*)data;
   settings_t        *settings = config_get_ptr();

   if (!gl)
      return;

   RARCH_LOG("[GL]: VSync => %s\n", state ? "off" : "on");

   context_bind_hw_render(false);

   if (!state)
      interval = settings->video.swap_interval;

   video_context_driver_swap_interval(&interval);
   context_bind_hw_render(true);
}

static bool resolve_extensions(gl_t *gl, const char *context_ident)
{
   settings_t *settings = config_get_ptr();
#ifndef HAVE_OPENGLES
   struct retro_hw_render_callback *hwr =
      video_driver_get_hw_context();

   gl_query_core_context_set(hwr->context_type == RETRO_HW_CONTEXT_OPENGL_CORE);
   
   if (gl_query_core_context_in_use())
   {
      RARCH_LOG("[GL]: Using Core GL context.\n");
      if (!gl_check_capability(GL_CAPS_VAO))
      {
         RARCH_ERR("[GL]: Failed to initialize VAOs.\n");
         return false;
      }
      glGenVertexArrays(1, &gl->vao);
   }

   /* GL_RGB565 internal format support.
    * Even though ES2 support is claimed, the format 
    * is not supported on older ATI catalyst drivers.
    *
    * The speed gain from using GL_RGB565 is worth 
    * adding some workarounds for.
    */
   gl->have_es2_compat        = gl_check_capability(GL_CAPS_ES2_COMPAT);
   gl->have_full_npot_support = gl_check_capability(GL_CAPS_FULL_NPOT_SUPPORT);
#endif

#ifdef HAVE_GL_SYNC
   gl->have_sync = gl_check_capability(GL_CAPS_SYNC);
   if (gl->have_sync && settings->video.hard_sync)
      RARCH_LOG("[GL]: Using ARB_sync to reduce latency.\n");
#endif

   video_driver_unset_rgba();
#if defined(HAVE_OPENGLES) && !defined(HAVE_PSGL)
   gl->have_full_npot_support = gl_check_capability(GL_CAPS_FULL_NPOT_SUPPORT);

   if (!gl_check_capability(GL_CAPS_BGRA8888))
   {
      video_driver_set_rgba();
      RARCH_WARN("[GL]: GLES implementation does not have BGRA8888 extension.\n"
                 "32-bit path will require conversion.\n");
   }

   /* GLES3 has unpack_subimage and sRGB in core. */
   gl->support_unpack_row_length = gl_check_capability(GL_CAPS_UNPACK_ROW_LENGTH);
   gl->has_srgb_fbo_gles3        = gl_check_capability(GL_CAPS_SRGB_FBO_ES3);

   /* TODO/FIXME - No extensions for float FBO currently. */
#endif

   gl->has_fp_fbo      = gl_check_capability(GL_CAPS_FP_FBO);
   if (settings->video.force_srgb_disable)
      gl->has_srgb_fbo = false;
   else
      gl->has_srgb_fbo = gl_check_capability(GL_CAPS_SRGB_FBO);

#ifdef GL_DEBUG
   /* Useful for debugging, but kinda obnoxious otherwise. */
   RARCH_LOG("[GL]: Supported extensions:\n");

   if (gl_query_core_context_in_use())
   {
#ifdef GL_NUM_EXTENSIONS
      GLint exts = 0;
      glGetIntegerv(GL_NUM_EXTENSIONS, &exts);
      for (GLint i = 0; i < exts; i++)
      {
         const char *ext = (const char*)glGetStringi(GL_EXTENSIONS, i);
         if (ext)
            RARCH_LOG("\t%s\n", ext);
      }
#endif
   }
   else
   {
      const char *ext = (const char*)glGetString(GL_EXTENSIONS);

      if (ext)
      {
         size_t i;
         struct string_list *list = string_split(ext, " ");

         for (i = 0; i < list->size; i++)
            RARCH_LOG("\t%s\n", list->elems[i].data);
         string_list_free(list);
      }
   }
#endif

   return true;
}

static INLINE void gl_set_texture_fmts(gl_t *gl, bool rgb32)
{
   gl->internal_fmt = RARCH_GL_INTERNAL_FORMAT16;
   gl->texture_type = RARCH_GL_TEXTURE_TYPE16;
   gl->texture_fmt  = RARCH_GL_FORMAT16;
   gl->base_size    = sizeof(uint16_t);

   if (rgb32)
   {
      bool use_rgba    = video_driver_supports_rgba();

      gl->internal_fmt = RARCH_GL_INTERNAL_FORMAT32;
      gl->texture_type = RARCH_GL_TEXTURE_TYPE32;
      gl->texture_fmt  = RARCH_GL_FORMAT32;
      gl->base_size    = sizeof(uint32_t);

      if (use_rgba)
      {
         gl->internal_fmt = GL_RGBA;
         gl->texture_type = GL_RGBA;
      }
   }
#ifndef HAVE_OPENGLES
   else if (gl->have_es2_compat)
   {
      RARCH_LOG("[GL]: Using GL_RGB565 for texture uploads.\n");
      gl->internal_fmt = RARCH_GL_INTERNAL_FORMAT16_565;
      gl->texture_type = RARCH_GL_TEXTURE_TYPE16_565;
      gl->texture_fmt  = RARCH_GL_FORMAT16_565;
   }
#endif
}

#ifdef HAVE_GL_ASYNC_READBACK
static void gl_init_pbo_readback(gl_t *gl)
{
   unsigned i;
   settings_t *settings      = config_get_ptr();
   bool *recording_enabled   = recording_is_enabled();
#ifndef HAVE_OPENGLES3
   struct scaler_ctx *scaler = NULL;
#endif

   /* Only bother with this if we're doing GPU recording.
    * Check recording_is_enabled() and not 
    * driver.recording_data, because recording is 
    * not initialized yet.
    */
   gl->pbo_readback_enable = settings->video.gpu_record 
      && *recording_enabled;

   if (!gl->pbo_readback_enable)
      return;

   RARCH_LOG("[GL]: Async PBO readback enabled.\n");

   glGenBuffers(4, gl->pbo_readback);
   for (i = 0; i < 4; i++)
   {
      glBindBuffer(GL_PIXEL_PACK_BUFFER, gl->pbo_readback[i]);
      glBufferData(GL_PIXEL_PACK_BUFFER, gl->vp.width * 
            gl->vp.height * sizeof(uint32_t),
            NULL, GL_STREAM_READ);
   }
   glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

#ifndef HAVE_OPENGLES3
   scaler              = &gl->pbo_readback_scaler;
   scaler->in_width    = gl->vp.width;
   scaler->in_height   = gl->vp.height;
   scaler->out_width   = gl->vp.width;
   scaler->out_height  = gl->vp.height;
   scaler->in_stride   = gl->vp.width * sizeof(uint32_t);
   scaler->out_stride  = gl->vp.width * 3;
   scaler->in_fmt      = SCALER_FMT_ARGB8888;
   scaler->out_fmt     = SCALER_FMT_BGR24;
   scaler->scaler_type = SCALER_TYPE_POINT;

   if (!scaler_ctx_gen_filter(scaler))
   {
      gl->pbo_readback_enable = false;
      RARCH_ERR("Failed to initialize pixel conversion for PBO.\n");
      glDeleteBuffers(4, gl->pbo_readback);
   }
#endif
}
#endif

static const gfx_ctx_driver_t *gl_get_context(gl_t *gl)
{
   enum gfx_ctx_api api;
   const char                 *api_name = NULL;
   settings_t                 *settings = config_get_ptr();
   struct retro_hw_render_callback *hwr = video_driver_get_hw_context();
   unsigned major                       = hwr->version_major;
   unsigned minor                       = hwr->version_minor;

#ifdef HAVE_OPENGLES
   api         = GFX_CTX_OPENGL_ES_API;
   api_name    = "OpenGL ES 2.0";

   if (hwr->context_type == RETRO_HW_CONTEXT_OPENGLES3)
   {
      major    = 3;
      minor    = 0;
      api_name = "OpenGL ES 3.0";
   }
   else if (hwr->context_type == RETRO_HW_CONTEXT_OPENGLES_VERSION)
      api_name = "OpenGL ES 3.1+";
#else
   api         = GFX_CTX_OPENGL_API;
   api_name    = "OpenGL";
#endif
    
   (void)api_name;

   gl_shared_context_use = settings->video.shared_context
      && hwr->context_type != RETRO_HW_CONTEXT_NONE;

   return video_context_driver_init_first(gl, settings->video.context_driver,
         api, major, minor, gl_shared_context_use);
}

#ifdef GL_DEBUG
#ifdef HAVE_GL_DEBUG_ES
#define DEBUG_CALLBACK_TYPE GL_APIENTRY

#define GL_DEBUG_SOURCE_API GL_DEBUG_SOURCE_API_KHR
#define GL_DEBUG_SOURCE_WINDOW_SYSTEM GL_DEBUG_SOURCE_WINDOW_SYSTEM_KHR
#define GL_DEBUG_SOURCE_SHADER_COMPILER GL_DEBUG_SOURCE_SHADER_COMPILER_KHR
#define GL_DEBUG_SOURCE_THIRD_PARTY GL_DEBUG_SOURCE_THIRD_PARTY_KHR
#define GL_DEBUG_SOURCE_APPLICATION GL_DEBUG_SOURCE_APPLICATION_KHR
#define GL_DEBUG_SOURCE_OTHER GL_DEBUG_SOURCE_OTHER_KHR
#define GL_DEBUG_TYPE_ERROR GL_DEBUG_TYPE_ERROR_KHR
#define GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_KHR
#define GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_KHR
#define GL_DEBUG_TYPE_PORTABILITY GL_DEBUG_TYPE_PORTABILITY_KHR
#define GL_DEBUG_TYPE_PERFORMANCE GL_DEBUG_TYPE_PERFORMANCE_KHR
#define GL_DEBUG_TYPE_MARKER GL_DEBUG_TYPE_MARKER_KHR
#define GL_DEBUG_TYPE_PUSH_GROUP GL_DEBUG_TYPE_PUSH_GROUP_KHR
#define GL_DEBUG_TYPE_POP_GROUP GL_DEBUG_TYPE_POP_GROUP_KHR
#define GL_DEBUG_TYPE_OTHER GL_DEBUG_TYPE_OTHER_KHR
#define GL_DEBUG_SEVERITY_HIGH GL_DEBUG_SEVERITY_HIGH_KHR
#define GL_DEBUG_SEVERITY_MEDIUM GL_DEBUG_SEVERITY_MEDIUM_KHR
#define GL_DEBUG_SEVERITY_LOW GL_DEBUG_SEVERITY_LOW_KHR
#else
#define DEBUG_CALLBACK_TYPE APIENTRY
#endif

static void DEBUG_CALLBACK_TYPE gl_debug_cb(GLenum source, GLenum type,
      GLuint id, GLenum severity, GLsizei length,
      const GLchar *message, void *userParam)
{
   const char      *src = NULL;
   const char *typestr  = NULL;
   gl_t             *gl = (gl_t*)userParam; /* Useful for debugger. */

   (void)gl;
   (void)id;
   (void)length;

   switch (source)
   {
      case GL_DEBUG_SOURCE_API:
         src = "API";
         break;
      case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
         src = "Window system";
         break;
      case GL_DEBUG_SOURCE_SHADER_COMPILER:
         src = "Shader compiler";
         break;
      case GL_DEBUG_SOURCE_THIRD_PARTY:
         src = "3rd party";
         break;
      case GL_DEBUG_SOURCE_APPLICATION:
         src = "Application";
         break;
      case GL_DEBUG_SOURCE_OTHER:
         src = "Other";
         break;
      default:
         src = "Unknown";
         break;
   }

   switch (type)
   {
      case GL_DEBUG_TYPE_ERROR:
         typestr = "Error";
         break;
      case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
         typestr = "Deprecated behavior";
         break;
      case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
         typestr = "Undefined behavior";
         break;
      case GL_DEBUG_TYPE_PORTABILITY:
         typestr = "Portability";
         break;
      case GL_DEBUG_TYPE_PERFORMANCE:
         typestr = "Performance";
         break;
      case GL_DEBUG_TYPE_MARKER:
         typestr = "Marker";
         break;
      case GL_DEBUG_TYPE_PUSH_GROUP:
         typestr = "Push group";
         break;
      case GL_DEBUG_TYPE_POP_GROUP:
        typestr = "Pop group";
        break;
      case GL_DEBUG_TYPE_OTHER:
        typestr = "Other";
        break;
      default:
        typestr = "Unknown";
        break;
   }

   switch (severity)
   {
      case GL_DEBUG_SEVERITY_HIGH:
         RARCH_ERR("[GL debug (High, %s, %s)]: %s\n", src, typestr, message);
         break;
      case GL_DEBUG_SEVERITY_MEDIUM:
         RARCH_WARN("[GL debug (Medium, %s, %s)]: %s\n", src, typestr, message);
         break;
      case GL_DEBUG_SEVERITY_LOW:
         RARCH_LOG("[GL debug (Low, %s, %s)]: %s\n", src, typestr, message);
         break;
   }
}

static void gl_begin_debug(gl_t *gl)
{
   if (gl_check_capability(GL_CAPS_DEBUG))
   {
#ifdef HAVE_GL_DEBUG_ES
      glDebugMessageCallbackKHR(gl_debug_cb, gl);
      glDebugMessageControlKHR(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
      glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_KHR);
#else
      glDebugMessageCallback(gl_debug_cb, gl);
      glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
      glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
#endif
   }
   else
      RARCH_ERR("Neither GL_KHR_debug nor GL_ARB_debug_output are implemented. Cannot start GL debugging.\n");
}
#endif


static void *gl_init(const video_info_t *video, const input_driver_t **input, void **input_data)
{
   gfx_ctx_mode_t mode;
   gfx_ctx_input_t inp;
   unsigned interval, mip_level;
   unsigned full_x, full_y;
   video_shader_ctx_filter_t shader_filter;
   video_shader_ctx_info_t shader_info;
   video_shader_ctx_ident_t ident_info;
   video_shader_ctx_wrap_t wrap_info    = {0};
   unsigned win_width                   = 0;
   unsigned win_height                  = 0;
   unsigned temp_width                  = 0;
   unsigned temp_height                 = 0;
   bool force_smooth                    = false;
   const char *vendor                   = NULL;
   const char *renderer                 = NULL;
   const char *version                  = NULL;
   struct retro_hw_render_callback *hwr = NULL;
   char *error_string                   = NULL;
   settings_t *settings                 = config_get_ptr();
   gl_t *gl                             = (gl_t*)calloc(1, sizeof(gl_t));
   const gfx_ctx_driver_t *ctx_driver   = gl_get_context(gl);
   if (!gl || !ctx_driver)
      goto error;

   video_context_driver_set((const gfx_ctx_driver_t*)ctx_driver);

   gl->video_info        = *video;

   RARCH_LOG("Found GL context: %s\n", ctx_driver->ident);

   video_context_driver_get_video_size(&mode);

   full_x  = mode.width;
   full_y  = mode.height;
   mode.width  = 0;
   mode.height = 0;

   RARCH_LOG("Detecting screen resolution %ux%u.\n", full_x, full_y);

   interval = video->vsync ? settings->video.swap_interval : 0;

   video_context_driver_swap_interval(&interval);

   win_width  = video->width;
   win_height = video->height;

   if (video->fullscreen && (win_width == 0) && (win_height == 0))
   {
      win_width  = full_x;
      win_height = full_y;
   }

   mode.width      = win_width;
   mode.height     = win_height;
   mode.fullscreen = video->fullscreen;

   if (!video_context_driver_set_video_mode(&mode))
      goto error;

   /* Clear out potential error flags in case we use cached context. */
   glGetError(); 

   vendor   = (const char*)glGetString(GL_VENDOR);
   renderer = (const char*)glGetString(GL_RENDERER);
   version  = (const char*)glGetString(GL_VERSION);

   RARCH_LOG("[GL]: Vendor: %s, Renderer: %s.\n", vendor, renderer);
   RARCH_LOG("[GL]: Version: %s.\n", version);

   if (!string_is_empty(version))
      sscanf(version, "%d.%d", &gl->version_major, &gl->version_minor);

#ifndef RARCH_CONSOLE
   rglgen_resolve_symbols(ctx_driver->get_proc_address);
#endif

   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glBlendEquation(GL_FUNC_ADD);

   if (!resolve_extensions(gl, ctx_driver->ident))
      goto error;

#ifdef GL_DEBUG
   gl_begin_debug(gl);
#endif

   gl->vsync      = video->vsync;
   gl->fullscreen = video->fullscreen;

   mode.width     = 0;
   mode.height    = 0;

   video_context_driver_get_video_size(&mode);

   temp_width     = mode.width;
   temp_height    = mode.height;
   mode.width     = 0;
   mode.height    = 0;
   
   /* Get real known video size, which might have been altered by context. */

   if (temp_width != 0 && temp_height != 0)
      video_driver_set_size(&temp_width, &temp_height);

   video_driver_get_size(&temp_width, &temp_height);

   RARCH_LOG("GL: Using resolution %ux%u\n", temp_width, temp_height);

   hwr = video_driver_get_hw_context();

   gl->vertex_ptr    = hwr->bottom_left_origin 
      ? vertexes : vertexes_flipped;

   /* Better pipelining with GPU due to synchronous glSubTexImage.
    * Multiple async PBOs would be an alternative,
    * but still need multiple textures with PREV.
    */
   gl->textures      = 4;
#ifdef HAVE_FBO
   gl->hw_render_use = hwr->context_type != RETRO_HW_CONTEXT_NONE;

   if (gl->hw_render_use)
   {
      /* All on GPU, no need to excessively
       * create textures. */
      gl->textures = 1;
#ifdef GL_DEBUG
      context_bind_hw_render(true);
      gl_begin_debug(gl);
      context_bind_hw_render(false);
#endif
   }
#endif
   gl->white_color_ptr = white_color;

#ifdef HAVE_GLSL
   gl_glsl_set_get_proc_address(ctx_driver->get_proc_address);
   gl_glsl_set_context_type(gl_query_core_context_in_use(),
         hwr->version_major, hwr->version_minor);
#endif

   if (!video_shader_driver_init_first())
      goto error;

   video_shader_driver_get_ident(&ident_info);

   RARCH_LOG("[GL]: Default shader backend found: %s.\n", ident_info.ident);

   if (!gl_shader_init(gl))
   {
      RARCH_ERR("[GL]: Shader initialization failed.\n");
      goto error;
   }

   {
      unsigned minimum;
      video_shader_ctx_texture_t texture_info;

      video_shader_driver_get_prev_textures(&texture_info);

      minimum          = texture_info.id;
      gl->textures     = MAX(minimum + 1, gl->textures);
   }

   if (!video_shader_driver_info(&shader_info))
      goto error;

   RARCH_LOG("[GL]: Using %u textures.\n", gl->textures);
   RARCH_LOG("[GL]: Loaded %u program(s).\n",
         shader_info.num);

   gl->tex_w = gl->tex_h = (RARCH_SCALE_BASE * video->input_scale);
   gl->keep_aspect     = video->force_aspect;

   /* Apparently need to set viewport for passes 
    * when we aren't using FBOs. */
   gl_set_shader_viewport(gl, 0);
   gl_set_shader_viewport(gl, 1);

   mip_level            = 1;
   gl->tex_mipmap       = video_shader_driver_mipmap_input(&mip_level);
   shader_filter.index  = 1;
   shader_filter.smooth = &force_smooth;

   if (video_shader_driver_filter_type(&shader_filter))
      gl->tex_min_filter = gl->tex_mipmap ? (force_smooth ? 
            GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST) 
         : (force_smooth ? GL_LINEAR : GL_NEAREST);
   else
      gl->tex_min_filter = gl->tex_mipmap ? 
         (video->smooth ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST) 
         : (video->smooth ? GL_LINEAR : GL_NEAREST);
   
   gl->tex_mag_filter = min_filter_to_mag(gl->tex_min_filter);

   wrap_info.idx      = 1;

   video_shader_driver_wrap_type(&wrap_info);

   gl->wrap_mode      = gl_wrap_type_to_enum(wrap_info.type);

   gl_set_texture_fmts(gl, video->rgb32);

#ifndef HAVE_OPENGLES
   if (!gl_query_core_context_in_use())
      glEnable(GL_TEXTURE_2D);
#endif

   glDisable(GL_DEPTH_TEST);
   glDisable(GL_CULL_FACE);
   glDisable(GL_DITHER);

   memcpy(gl->tex_info.coord, tex_coords, sizeof(gl->tex_info.coord));
   gl->coords.vertex         = gl->vertex_ptr;
   gl->coords.tex_coord      = gl->tex_info.coord;
   gl->coords.color          = gl->white_color_ptr;
   gl->coords.lut_tex_coord  = tex_coords;
   gl->coords.vertices       = 4;

   /* Empty buffer that we use to clear out 
    * the texture with on res change. */
   gl->empty_buf             = calloc(sizeof(uint32_t), gl->tex_w * gl->tex_h);

#if !defined(HAVE_PSGL)
   gl->conv_buffer           = calloc(sizeof(uint32_t), gl->tex_w * gl->tex_h);

   if (!gl->conv_buffer)
      goto error;
#endif

   gl_init_textures(gl, video);
   gl_init_textures_data(gl);

#ifdef HAVE_FBO
   gl_renderchain_init(gl, gl->tex_w, gl->tex_h);

   if (gl->hw_render_use && 
         !gl_init_hw_render(gl, gl->tex_w, gl->tex_h))
      goto error;
#endif

   inp.input      = input;
   inp.input_data = input_data;

   video_context_driver_input_driver(&inp);
   
   if (settings->video.font_enable)
   {
      if (!font_driver_init_first(NULL, NULL, gl, *settings->path.font 
            ? settings->path.font : NULL, settings->video.font_size, false,
            FONT_DRIVER_RENDER_OPENGL_API))
         RARCH_ERR("[GL]: Failed to initialize font renderer.\n");
   }

#ifdef HAVE_GL_ASYNC_READBACK
   gl_init_pbo_readback(gl);
#endif

   if (!gl_check_error(error_string))
   {
      RARCH_ERR("%s\n", error_string);
      free(error_string);
      goto error;
   }

   context_bind_hw_render(true);
   return gl;

error:
   video_context_driver_destroy();
   gl_destroy_resources(gl);
   return NULL;
}

static bool gl_alive(void *data)
{
   gfx_ctx_size_t size_data;
   unsigned temp_width  = 0;
   unsigned temp_height = 0;
   bool ret             = false;
   bool quit            = false;
   bool resize          = false;
   gl_t         *gl     = (gl_t*)data;

   /* Needed because some context drivers don't track their sizes */
   video_driver_get_size(&temp_width, &temp_height);

   size_data.quit       = &quit;
   size_data.resize     = &resize;
   size_data.width      = &temp_width;
   size_data.height     = &temp_height;

   if (video_context_driver_check_window(&size_data))
   {
      if (quit)
         gl->quitting = true;
      else if (resize)
         gl->should_resize = true;

      ret = !gl->quitting;
   }

   if (temp_width != 0 && temp_height != 0)
      video_driver_set_size(&temp_width, &temp_height);

   return ret;
}

static bool gl_focus(void *data)
{
   return video_context_driver_focus();
}

static bool gl_suppress_screensaver(void *data, bool enable)
{
   bool enabled = enable;
   return video_context_driver_suppress_screensaver(&enabled);
}

static bool gl_has_windowed(void *data)
{
   return video_context_driver_has_windowed();
}

static void gl_update_tex_filter_frame(gl_t *gl)
{
   video_shader_ctx_filter_t shader_filter;
   unsigned i, mip_level;
   GLenum wrap_mode;
   GLuint new_filt;
   video_shader_ctx_wrap_t wrap_info = {0};
   bool smooth                       = false;
   settings_t *settings              = config_get_ptr();

   if (!gl)
      return;

   context_bind_hw_render(false);

   shader_filter.index  = 1;
   shader_filter.smooth = &smooth;

   if (!video_shader_driver_filter_type(&shader_filter))
      smooth = settings->video.smooth;

   mip_level             = 1;
   wrap_info.idx         = 1;

   video_shader_driver_wrap_type(&wrap_info);

   wrap_mode             = gl_wrap_type_to_enum(wrap_info.type);
   gl->tex_mipmap        = video_shader_driver_mipmap_input(&mip_level);

   gl->video_info.smooth = smooth;
   new_filt = gl->tex_mipmap ? (smooth ? 
         GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST) 
      : (smooth ? GL_LINEAR : GL_NEAREST);

   if (new_filt == gl->tex_min_filter && wrap_mode == gl->wrap_mode)
      return;

   gl->tex_min_filter    = new_filt;
   gl->tex_mag_filter    = min_filter_to_mag(gl->tex_min_filter);
   gl->wrap_mode         = wrap_mode;

   for (i = 0; i < gl->textures; i++)
   {
      if (!gl->texture[i])
         continue;

      glBindTexture(GL_TEXTURE_2D, gl->texture[i]);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl->wrap_mode);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl->wrap_mode);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl->tex_mag_filter);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl->tex_min_filter);
   }

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
   context_bind_hw_render(true);
}

static bool gl_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
#if defined(HAVE_GLSL) || defined(HAVE_CG)
   unsigned textures;
   video_shader_ctx_texture_t texture_info;
   video_shader_ctx_init_t init_data;
   gl_t *gl = (gl_t*)data;

   if (!gl)
      return false;

   context_bind_hw_render(false);

   if (type == RARCH_SHADER_NONE)
      return false;

   video_shader_driver_deinit();

   switch (type)
   {
#ifdef HAVE_GLSL
      case RARCH_SHADER_GLSL:
         break;
#endif

#ifdef HAVE_CG
      case RARCH_SHADER_CG:
         break;
#endif

      default:
         RARCH_ERR("[GL]: Cannot find shader core for path: %s.\n", path);
         goto error;
   }

#ifdef HAVE_FBO
   gl_deinit_fbo(gl);
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
#endif

   init_data.shader_type = type;
   init_data.shader      = NULL;
   init_data.data        = gl;
   init_data.path        = path;

   if (!video_shader_driver_init(&init_data))
   {
      init_data.path = NULL;

      video_shader_driver_init(&init_data);

      RARCH_WARN("[GL]: Failed to set multipass shader. Falling back to stock.\n");

      goto error;
   }

   gl_update_tex_filter_frame(gl);

   video_shader_driver_get_prev_textures(&texture_info);

   textures = texture_info.id + 1;

   if (textures > gl->textures) /* Have to reinit a bit. */
   {
#if defined(HAVE_FBO)
      gl_deinit_hw_render(gl);
#endif

      glDeleteTextures(gl->textures, gl->texture);
#if defined(HAVE_PSGL)
      glBindBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, 0);
      glDeleteBuffers(1, &gl->pbo);
#endif
      gl->textures = textures;
      RARCH_LOG("[GL]: Using %u textures.\n", gl->textures);
      gl->tex_index = 0;
      gl_init_textures(gl, &gl->video_info);
      gl_init_textures_data(gl);

#if defined(HAVE_FBO)
      if (gl->hw_render_use)
         gl_init_hw_render(gl, gl->tex_w, gl->tex_h);
#endif
   }

#ifdef HAVE_FBO
   gl_renderchain_init(gl, gl->tex_w, gl->tex_h);
#endif

   /* Apparently need to set viewport for passes when we aren't using FBOs. */
   gl_set_shader_viewport(gl, 0);
   gl_set_shader_viewport(gl, 1);
   context_bind_hw_render(true);
#if defined(_WIN32) && !defined(_XBOX)
   shader_dlg_params_reload();
#endif

#endif

   return true;

error:
   context_bind_hw_render(true);
   return false;
}

static void gl_viewport_info(void *data, struct video_viewport *vp)
{
   unsigned width, height;
   unsigned top_y, top_dist;
   gl_t *gl         = (gl_t*)data;

   video_driver_get_size(&width, &height);

   *vp             = gl->vp;
   vp->full_width  = width;
   vp->full_height = height;

   /* Adjust as GL viewport is bottom-up. */
   top_y           = vp->y + vp->height;
   top_dist        = height - top_y;
   vp->y           = top_dist;
}

static bool gl_read_viewport(void *data, uint8_t *buffer)
{
#ifndef NO_GL_READ_PIXELS
   static struct retro_perf_counter read_viewport = {0};
   unsigned                     num_pixels = 0;
   gl_t                                *gl = (gl_t*)data;

   if (!gl)
      return false;

   context_bind_hw_render(false);

   performance_counter_init(&read_viewport, "read_viewport");
   performance_counter_start(&read_viewport);

   num_pixels = gl->vp.width * gl->vp.height;

#ifdef HAVE_GL_ASYNC_READBACK
   if (gl->pbo_readback_enable)
   {
      const uint8_t *ptr  = NULL;

      /* Don't readback if we're in menu mode.
       * We haven't buffered up enough frames yet, come back later. */
      if (!gl->pbo_readback_valid[gl->pbo_readback_index]) 
         goto error;

      gl->pbo_readback_valid[gl->pbo_readback_index] = false;
      glBindBuffer(GL_PIXEL_PACK_BUFFER,
            gl->pbo_readback[gl->pbo_readback_index]);

#ifdef HAVE_OPENGLES3
      /* Slower path, but should work on all implementations at least. */
      ptr        = (const uint8_t*)glMapBufferRange(GL_PIXEL_PACK_BUFFER,
            0, num_pixels * sizeof(uint32_t), GL_MAP_READ_BIT);

      if (ptr)
      {
         unsigned y;
         for (y = 0; y < gl->vp.height; y++)
         {
            video_frame_convert_rgba_to_bgr(
                  (const void*)ptr,
                  buffer,
                  gl->vp.width);
         }
      }
#else
      ptr = (const uint8_t*)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
      if (ptr)
         scaler_ctx_scale(&gl->pbo_readback_scaler, buffer, ptr);
#endif

      if (!ptr)
      {
         RARCH_ERR("[GL]: Failed to map pixel unpack buffer.\n");
         goto error;
      }

      glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
      glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
   }
   else /* Use slow synchronous readbacks. Use this with plain screenshots 
           as we don't really care about performance in this case. */
#endif
   {
      /* GLES2 only guarantees GL_RGBA/GL_UNSIGNED_BYTE 
       * readbacks so do just that.
       * GLES2 also doesn't support reading back data 
       * from front buffer, so render a cached frame 
       * and have gl_frame() do the readback while it's 
       * in the back buffer.
       *
       * Keep codepath similar for GLES and desktop GL.
       */
      gl->readback_buffer_screenshot = malloc(num_pixels * sizeof(uint32_t));

      if (!gl->readback_buffer_screenshot)
      {
         performance_counter_stop(&read_viewport);
         goto error;
      }

      video_driver_cached_frame_render();

      video_frame_convert_rgba_to_bgr(
            (const void*)gl->readback_buffer_screenshot,
            buffer,
            num_pixels);

      free(gl->readback_buffer_screenshot);
      gl->readback_buffer_screenshot = NULL;
   }

   performance_counter_stop(&read_viewport);
   context_bind_hw_render(true);
   return true;

error:
   context_bind_hw_render(true);
#endif

   return false;
}

#if 0
#define READ_RAW_GL_FRAME_TEST
#endif

#if defined(READ_RAW_GL_FRAME_TEST)
static void* gl_read_frame_raw(void *data, unsigned *width_p,
unsigned *height_p, size_t *pitch_p)
{
   gl_t *gl             = (gl_t*)data;
   unsigned width       = gl->last_width[gl->tex_index];
   unsigned height      = gl->last_height[gl->tex_index];
   size_t pitch         = gl->tex_w * gl->base_size;
   void* buffer         = NULL;
   void* buffer_texture = NULL;

#ifdef HAVE_FBO
   if (gl->hw_render_use)
   {
      buffer = malloc(pitch * height);
      if (!buffer)
         return NULL;
   }
#endif

   buffer_texture = malloc(pitch * gl->tex_h);

   if (!buffer_texture)
   {
      if (buffer)
         free(buffer);
      return NULL;
   }

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
   glGetTexImage(GL_TEXTURE_2D, 0,
         gl->texture_type, gl->texture_fmt, buffer_texture);

   *width_p  = width;
   *height_p = height;
   *pitch_p  = pitch;

#ifdef HAVE_FBO
   if (gl->hw_render_use)
   {
      unsigned i;

      for(i = 0; i < height ; i++)
         memcpy((uint8_t*)buffer + i * pitch,
            (uint8_t*)buffer_texture + (height - 1 - i) * pitch, pitch);

      free(buffer_texture);
      return buffer;
   }
#endif
   return buffer_texture;
}
#endif


bool gl_load_luts(const struct video_shader *shader,
      GLuint *textures_lut)
{
   unsigned i;
   unsigned num_luts = MIN(shader->luts, GFX_MAX_TEXTURES);

   if (!shader->luts)
      return true;

   glGenTextures(num_luts, textures_lut);

   for (i = 0; i < num_luts; i++)
   {
      if (!gl_renderchain_add_lut(shader, i, textures_lut))
         return false;
   }

   glBindTexture(GL_TEXTURE_2D, 0);
   return true;
}

#ifdef HAVE_OVERLAY
static bool gl_overlay_load(void *data, 
      const void *image_data, unsigned num_images)
{
   unsigned i, j;
   gl_t *gl = (gl_t*)data;
   const struct texture_image *images = 
      (const struct texture_image*)image_data;

   if (!gl)
      return false;

   context_bind_hw_render(false);

   gl_free_overlay(gl);
   gl->overlay_tex = (GLuint*)calloc(num_images, sizeof(*gl->overlay_tex));

   if (!gl->overlay_tex)
   {
      context_bind_hw_render(true);
      return false;
   }

   gl->overlay_vertex_coord = (GLfloat*)
      calloc(2 * 4 * num_images, sizeof(GLfloat));
   gl->overlay_tex_coord    = (GLfloat*)
      calloc(2 * 4 * num_images, sizeof(GLfloat));
   gl->overlay_color_coord  = (GLfloat*)
      calloc(4 * 4 * num_images, sizeof(GLfloat));

   if (     !gl->overlay_vertex_coord 
         || !gl->overlay_tex_coord 
         || !gl->overlay_color_coord)
      return false;

   gl->overlays             = num_images;
   glGenTextures(num_images, gl->overlay_tex);

   for (i = 0; i < num_images; i++)
   {
      unsigned alignment = video_pixel_get_alignment(images[i].width 
            * sizeof(uint32_t));

      gl_load_texture_data(gl->overlay_tex[i],
            RARCH_WRAP_EDGE, TEXTURE_FILTER_LINEAR,
            alignment,
            images[i].width, images[i].height, images[i].pixels,
            sizeof(uint32_t));

      /* Default. Stretch to whole screen. */
      gl_overlay_tex_geom(gl, i, 0, 0, 1, 1);
      gl_overlay_vertex_geom(gl, i, 0, 0, 1, 1);

      for (j = 0; j < 16; j++)
         gl->overlay_color_coord[16 * i + j] = 1.0f;
   }

   context_bind_hw_render(true);
   return true;
}



static void gl_overlay_enable(void *data, bool state)
{
   gl_t *gl           = (gl_t*)data;

   if (!gl)
      return;

   gl->overlay_enable = state;

   if (gl->fullscreen)
      video_context_driver_show_mouse(&state);
}

static void gl_overlay_full_screen(void *data, bool enable)
{
   gl_t *gl = (gl_t*)data;

   if (gl)
      gl->overlay_full_screen = enable;
}

static void gl_overlay_set_alpha(void *data, unsigned image, float mod)
{
   GLfloat *color = NULL;
   gl_t *gl       = (gl_t*)data;
   if (!gl)
      return;

   color          = (GLfloat*)&gl->overlay_color_coord[image * 16];

   color[ 0 + 3]  = mod;
   color[ 4 + 3]  = mod;
   color[ 8 + 3]  = mod;
   color[12 + 3]  = mod;
}


static const video_overlay_interface_t gl_overlay_interface = {
   gl_overlay_enable,
   gl_overlay_load,
   gl_overlay_tex_geom,
   gl_overlay_vertex_geom,
   gl_overlay_full_screen,
   gl_overlay_set_alpha,
};

static void gl_get_overlay_interface(void *data,
      const video_overlay_interface_t **iface)
{
   (void)data;
   *iface = &gl_overlay_interface;
}
#endif


static retro_proc_address_t gl_get_proc_address(void *data, const char *sym)
{
   gfx_ctx_proc_address_t proc_address = {0};

   proc_address.sym = sym;

   video_context_driver_get_proc_address(&proc_address);

   return proc_address.addr;
}

static void gl_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   gl_t *gl         = (gl_t*)data;

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

   video_driver_set_aspect_ratio_value(
         aspectratio_lut[aspect_ratio_idx].value);

   if (!gl)
      return;

   gl->keep_aspect = true;
   gl->should_resize = true;
}


static void gl_apply_state_changes(void *data)
{
   gl_t *gl = (gl_t*)data;

   if (gl)
      gl->should_resize = true;
}


static void gl_get_video_output_size(void *data,
      unsigned *width, unsigned *height)
{
   gfx_ctx_size_t size_data;
   size_data.width  = width;
   size_data.height = height;
   video_context_driver_get_video_output_size(&size_data);
}

static void gl_get_video_output_prev(void *data)
{
   video_context_driver_get_video_output_prev();
}

static void gl_get_video_output_next(void *data)
{
   video_context_driver_get_video_output_next();
}

static void video_texture_load_gl(
      struct texture_image *ti,
      enum texture_filter_type filter_type,
      uintptr_t *id)
{
   /* Generate the OpenGL texture object */
   glGenTextures(1, (GLuint*)id);
   gl_load_texture_data((GLuint)*id, 
         RARCH_WRAP_EDGE, filter_type,
         4 /* TODO/FIXME - dehardcode */,
         ti->width, ti->height, ti->pixels,
         sizeof(uint32_t) /* TODO/FIXME - dehardcode */
         );
}

#ifdef HAVE_THREADS
static int video_texture_load_wrap_gl_mipmap(void *data)
{
   uintptr_t id = 0;

   if (!data)
      return 0;
   video_texture_load_gl((struct texture_image*)data,
         TEXTURE_FILTER_MIPMAP_LINEAR, &id);
   return id;
}

static int video_texture_load_wrap_gl(void *data)
{
   uintptr_t id = 0;

   if (!data)
      return 0;
   video_texture_load_gl((struct texture_image*)data,
         TEXTURE_FILTER_LINEAR, &id);
   return id;
}
#endif

static uintptr_t gl_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
   uintptr_t id = 0;

#ifdef HAVE_THREADS
   if (threaded)
   {
      custom_command_method_t func = video_texture_load_wrap_gl;

      switch (filter_type)
      {
         case TEXTURE_FILTER_MIPMAP_LINEAR:
         case TEXTURE_FILTER_MIPMAP_NEAREST:
            func = video_texture_load_wrap_gl_mipmap;
            break;
         default:
            break;
      }
      return video_thread_texture_load(data, func);
   }
#endif

   video_texture_load_gl((struct texture_image*)data, filter_type, &id);
   return id;
}

static void gl_unload_texture(void *data, uintptr_t id)
{
   GLuint glid;
   if (!id)
      return;

   glid = (GLuint)id;
   glDeleteTextures(1, &glid);
}

static const video_poke_interface_t gl_poke_interface = {
   gl_load_texture,
   gl_unload_texture,
   gl_set_video_mode,
   NULL,
   gl_get_video_output_size,
   gl_get_video_output_prev,
   gl_get_video_output_next,
#ifdef HAVE_FBO
   gl_get_current_framebuffer,
#else
   NULL,
#endif
   gl_get_proc_address,
   gl_set_aspect_ratio,
   gl_apply_state_changes,
#if defined(HAVE_MENU)
   gl_set_texture_frame,
   gl_set_texture_enable,
   gl_set_osd_msg,
   gl_show_mouse,
#else
   NULL,
   NULL,
   NULL,
   NULL,
#endif

   NULL,
#ifdef HAVE_MENU
   gl_get_current_shader,
#endif
};

static void gl_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &gl_poke_interface;
}

video_driver_t video_gl = {
   gl_init,
   gl_frame,
   gl_set_nonblock_state,
   gl_alive,
   gl_focus,
   gl_suppress_screensaver,
   gl_has_windowed,

   gl_set_shader,

   gl_free,
   "gl",

   gl_set_viewport,
   gl_set_rotation,

   gl_viewport_info,

   gl_read_viewport,
#if defined(READ_RAW_GL_FRAME_TEST)
   gl_read_frame_raw,
#else
   NULL,
#endif

#ifdef HAVE_OVERLAY
   gl_get_overlay_interface,
#endif
   gl_get_poke_interface,
   gl_wrap_type_to_enum,
};

