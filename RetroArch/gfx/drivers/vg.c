/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include <math.h>

#include <VG/openvg.h>
#include <VG/vgext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <retro_inline.h>
#include <retro_assert.h>
#include <gfx/math/matrix_3x3.h>
#include <libretro.h>

#include "../video_context_driver.h"
#include "../../general.h"
#include "../../retroarch.h"
#include "../../driver.h"
#include "../../performance_counters.h"
#include "../font_driver.h"
#include "../../content.h"
#include "../../runloop.h"
#include "../../verbosity.h"
#include "../../configuration.h"

typedef struct
{
   bool should_resize;
   float mScreenAspect;
   bool keep_aspect;
   bool mEglImageBuf;
   unsigned mTextureWidth;
   unsigned mTextureHeight;
   unsigned mRenderWidth;
   unsigned mRenderHeight;
   unsigned x1, y1, x2, y2;
   VGImageFormat mTexType;
   VGImage mImage;
   math_matrix_3x3 mTransformMatrix;
   VGint scissor[4];
   EGLImageKHR last_egl_image;

   char *mLastMsg;
   uint32_t mFontHeight;
   VGFont mFont;
   void *mFontRenderer;
   const font_renderer_driver_t *font_driver;
   bool mFontsOn;
   VGuint mMsgLength;
   VGuint mGlyphIndices[1024];
   VGPaint mPaintFg;
   VGPaint mPaintBg;
} vg_t;

static PFNVGCREATEEGLIMAGETARGETKHRPROC pvgCreateEGLImageTargetKHR;

static void vg_set_nonblock_state(void *data, bool state)
{
   unsigned interval = state ? 0 : 1;
   video_context_driver_swap_interval(&interval);
}

static INLINE bool vg_query_extension(const char *ext)
{
   const char *str = (const char*)vgGetString(VG_EXTENSIONS);
   bool ret = str && strstr(str, ext);
   RARCH_LOG("Querying VG extension: %s => %s\n",
         ext, ret ? "exists" : "doesn't exist");

   return ret;
}

static void *vg_init(const video_info_t *video,
      const input_driver_t **input, void **input_data)
{
   gfx_ctx_mode_t mode;
   gfx_ctx_input_t inp;
   gfx_ctx_aspect_t aspect_data;
   unsigned interval;
   unsigned temp_width = 0, temp_height = 0;
   VGfloat clearColor[4] = {0, 0, 0, 1};
   settings_t        *settings = config_get_ptr();
   vg_t                    *vg = (vg_t*)calloc(1, sizeof(vg_t));
   const gfx_ctx_driver_t *ctx = video_context_driver_init_first(
         vg, settings->video.context_driver,
         GFX_CTX_OPENVG_API, 0, 0, false);

   if (!vg || !ctx)
      goto error;

   video_context_driver_set((void*)ctx);

   video_context_driver_get_video_size(&mode);

   temp_width  = mode.width;
   temp_height = mode.height;
   mode.width  = 0;
   mode.height = 0;

   RARCH_LOG("Detecting screen resolution %ux%u.\n", temp_width, temp_height);

   if (temp_width != 0 && temp_height != 0)
      video_driver_set_size(&temp_width, &temp_height);

   interval = video->vsync ? 1 : 0;

   video_context_driver_swap_interval(&interval);
   video_context_driver_update_window_title();

   vg->mTexType    = video->rgb32 ? VG_sXRGB_8888 : VG_sRGB_565;
   vg->keep_aspect = video->force_aspect;

   unsigned win_width  = video->width;
   unsigned win_height = video->height;
   if (video->fullscreen && (win_width == 0) && (win_height == 0))
   {
      video_driver_get_size(&temp_width, &temp_height);

      win_width  = temp_width;
      win_height = temp_height;
   }

   mode.width      = win_width;
   mode.height     = win_height;
   mode.fullscreen = video->fullscreen;

   if (!video_context_driver_set_video_mode(&mode))
      goto error;

   video_driver_get_size(&temp_width, &temp_height);

   temp_width  = 0;
   temp_height = 0;
   mode.width  = 0;
   mode.height = 0;

   video_context_driver_get_video_size(&mode);

   temp_width  = mode.width;
   temp_height = mode.height;
   mode.width  = 0;
   mode.height = 0;

   vg->should_resize = true;

   if (temp_width != 0 && temp_height != 0)
   {
      RARCH_LOG("Verified window resolution %ux%u.\n", temp_width, temp_height);
      video_driver_set_size(&temp_width, &temp_height);
   }

   video_driver_get_size(&temp_width, &temp_height);

   vg->mScreenAspect = (float)temp_width / temp_height;

   aspect_data.aspect   = &vg->mScreenAspect;
   aspect_data.width    = temp_width;
   aspect_data.height   = temp_height;

   video_context_driver_translate_aspect(&aspect_data);

   vgSetfv(VG_CLEAR_COLOR, 4, clearColor);

   vg->mTextureWidth = vg->mTextureHeight = video->input_scale * RARCH_SCALE_BASE;
   vg->mImage = vgCreateImage(vg->mTexType, vg->mTextureWidth, vg->mTextureHeight,
         video->smooth ? VG_IMAGE_QUALITY_BETTER : VG_IMAGE_QUALITY_NONANTIALIASED);
   vg_set_nonblock_state(vg, !video->vsync);

   inp.input      = input;
   inp.input_data = input_data;

   video_context_driver_input_driver(&inp);

   if (     settings->video.font_enable 
         && font_renderer_create_default((const void**)&vg->font_driver, &vg->mFontRenderer,
            *settings->path.font ? settings->path.font : NULL, settings->video.font_size))
   {
      vg->mFont            = vgCreateFont(0);

      if (vg->mFont != VG_INVALID_HANDLE)
      {
         vg->mFontsOn      = true;
         vg->mFontHeight   = settings->video.font_size;
         vg->mPaintFg      = vgCreatePaint();
         vg->mPaintBg      = vgCreatePaint();
         VGfloat paintFg[] = { 
            settings->video.msg_color_r,
            settings->video.msg_color_g,
            settings->video.msg_color_b,
            1.0f };
         VGfloat paintBg[] = { 
            settings->video.msg_color_r / 2.0f,
            settings->video.msg_color_g / 2.0f,
            settings->video.msg_color_b / 2.0f,
            0.5f };

         vgSetParameteri(vg->mPaintFg, VG_PAINT_TYPE, VG_PAINT_TYPE_COLOR);
         vgSetParameterfv(vg->mPaintFg, VG_PAINT_COLOR, 4, paintFg);

         vgSetParameteri(vg->mPaintBg, VG_PAINT_TYPE, VG_PAINT_TYPE_COLOR);
         vgSetParameterfv(vg->mPaintBg, VG_PAINT_COLOR, 4, paintBg);
      }
   }

   if (vg_query_extension("KHR_EGL_image") 
         && video_context_driver_init_image_buffer((void*)video))
   {
      gfx_ctx_proc_address_t proc_address;

      proc_address.sym = "vgCreateEGLImageTargetKHR";

      video_context_driver_get_proc_address(&proc_address);

      pvgCreateEGLImageTargetKHR = 
         (PFNVGCREATEEGLIMAGETARGETKHRPROC)proc_address.addr;

      if (pvgCreateEGLImageTargetKHR)
      {
         RARCH_LOG("[VG] Using EGLImage buffer\n");
         vg->mEglImageBuf = true;
      }
   }

#if 0
   const char *ext = (const char*)vgGetString(VG_EXTENSIONS);
   if (ext)
      RARCH_LOG("[VG] Supported extensions: %s\n", ext);
#endif

   return vg;

error:
   if (vg)
      free(vg);
   video_context_driver_destroy();
   return NULL;
}

static void vg_free(void *data)
{
   vg_t                    *vg = (vg_t*)data;

   if (!vg)
      return;

   vgDestroyImage(vg->mImage);

   if (vg->mFontsOn)
   {
      vgDestroyFont(vg->mFont);
      vg->font_driver->free(vg->mFontRenderer);
      vgDestroyPaint(vg->mPaintFg);
      vgDestroyPaint(vg->mPaintBg);
   }

   video_context_driver_free();

   free(vg);
}

static void vg_calculate_quad(vg_t *vg)
{
   unsigned width, height;
   video_driver_get_size(&width, &height);

   /* set viewport for aspect ratio, taken from the OpenGL driver. */
   if (vg->keep_aspect)
   {
      float desired_aspect = video_driver_get_aspect_ratio();

      /* If the aspect ratios of screen and desired aspect ratio 
       * are sufficiently equal (floating point stuff),
       * assume they are actually equal. */
      if (fabs(vg->mScreenAspect - desired_aspect) < 0.0001)
      {
         vg->x1 = 0;
         vg->y1 = 0;
         vg->x2 = width;
         vg->y2 = height;
      }
      else if (vg->mScreenAspect > desired_aspect)
      {
         float delta = (desired_aspect / vg->mScreenAspect - 1.0) / 2.0 + 0.5;
         vg->x1 = width * (0.5 - delta);
         vg->y1 = 0;
         vg->x2 = 2.0 * width * delta + vg->x1;
         vg->y2 = height + vg->y1;
      }
      else
      {
         float delta = (vg->mScreenAspect / desired_aspect - 1.0) / 2.0 + 0.5;
         vg->x1 = 0;
         vg->y1 = height * (0.5 - delta);
         vg->x2 = width + vg->x1;
         vg->y2 = 2.0 * height * delta + vg->y1;
      }
   }
   else
   {
      vg->x1 = 0;
      vg->y1 = 0;
      vg->x2 = width;
      vg->y2 = height;
   }

   vg->scissor[0] = vg->x1;
   vg->scissor[1] = vg->y1;
   vg->scissor[2] = vg->x2 - vg->x1;
   vg->scissor[3] = vg->y2 - vg->y1;

   vgSetiv(VG_SCISSOR_RECTS, 4, vg->scissor);
}

static void vg_copy_frame(void *data, const void *frame,
      unsigned width, unsigned height, unsigned pitch)
{
   vg_t *vg = (vg_t*)data;

   if (vg->mEglImageBuf)
   {
      gfx_ctx_image_t img_info;
      EGLImageKHR img = 0;
      bool new_egl    = false;

      img_info.frame  = frame;
      img_info.width  = width;
      img_info.height = height;
      img_info.pitch  = pitch;
      img_info.rgb32  = (vg->mTexType == VG_sXRGB_8888);
      img_info.index  = 0;
      img_info.handle = &img;
      
      new_egl         = video_context_driver_write_to_image_buffer(&img_info);
      
      retro_assert(img != EGL_NO_IMAGE_KHR);

      if (new_egl)
      {
         vgDestroyImage(vg->mImage);
         vg->mImage = pvgCreateEGLImageTargetKHR((VGeglImageKHR) img);
         if (!vg->mImage)
         {
            RARCH_ERR(
                  "[VG:EGLImage] Error creating image: %08x\n",
                  vgGetError());
            exit(2);
         }
         vg->last_egl_image = img;
      }
   }
   else
      vgImageSubData(vg->mImage, frame, pitch, vg->mTexType, 0, 0, width, height);
}

static bool vg_frame(void *data, const void *frame,
      unsigned frame_width, unsigned frame_height,
      uint64_t frame_count, unsigned pitch, const char *msg)
{
   unsigned width, height;
   vg_t                           *vg = (vg_t*)data;
   static struct retro_perf_counter    vg_fr = {0};
   static struct retro_perf_counter vg_image = {0};

   performance_counter_init(&vg_fr, "vg_fr");
   performance_counter_start(&vg_fr);

   video_driver_get_size(&width, &height);

   if (     frame_width != vg->mRenderWidth 
         || frame_height != vg->mRenderHeight 
         || vg->should_resize)
   {
      vg->mRenderWidth  = frame_width;
      vg->mRenderHeight = frame_height;
      vg_calculate_quad(vg);
      matrix_3x3_quad_to_quad(
         vg->x1, vg->y1, vg->x2, vg->y1, vg->x2, vg->y2, vg->x1, vg->y2,
         /* needs to be flipped, Khronos loves their bottom-left origin */
         0, frame_height, frame_width, frame_height, frame_width, 0, 0, 0,
         &vg->mTransformMatrix);
      vgSeti(VG_MATRIX_MODE, VG_MATRIX_IMAGE_USER_TO_SURFACE);
      vgLoadMatrix(vg->mTransformMatrix.data);

      vg->should_resize = false;
   }

   vgSeti(VG_SCISSORING, VG_FALSE);
   vgClear(0, 0, width, height);
   vgSeti(VG_SCISSORING, VG_TRUE);

   performance_counter_init(&vg_image, "vg_image");
   performance_counter_start(&vg_image);
   vg_copy_frame(vg, frame, frame_width, frame_height, pitch);
   performance_counter_stop(&vg_image);

   vgDrawImage(vg->mImage);

#if 0
   if (msg && vg->mFontsOn)
      vg_draw_message(vg, msg);
#endif

   video_context_driver_update_window_title();

   performance_counter_stop(&vg_fr);

   video_context_driver_swap_buffers();

   return true;
}

static bool vg_alive(void *data)
{
   gfx_ctx_size_t size_data;
   bool quit            = false;
   unsigned temp_width  = 0;
   unsigned temp_height = 0;
   vg_t            *vg  = (vg_t*)data;

   size_data.quit       = &quit;
   size_data.resize     = &vg->should_resize;
   size_data.width      = &temp_width;
   size_data.height     = &temp_height;

   video_context_driver_check_window(&size_data);

   if (temp_width != 0 && temp_height != 0)
      video_driver_set_size(&temp_width, &temp_height);

   return !quit;
}

static bool vg_focus(void *data)
{
   return video_context_driver_focus();
}

static bool vg_suppress_screensaver(void *data, bool enable)
{
   bool enabled = enable;
   return video_context_driver_suppress_screensaver(&enabled);
}

static bool vg_has_windowed(void *data)
{
   return video_context_driver_has_windowed();
}

static bool vg_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   (void)data;
   (void)type;
   (void)path;

   return false; 
}

static void vg_set_rotation(void *data, unsigned rotation)
{
   (void)data;
   (void)rotation;
}

static void vg_viewport_info(void *data,
      struct video_viewport *vp)
{
   (void)data;
   (void)vp;
}

static bool vg_read_viewport(void *data, uint8_t *buffer)
{
   (void)data;
   (void)buffer;

   return true;
}

static void vg_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   (void)iface;
}

video_driver_t video_vg = {
   vg_init,
   vg_frame,
   vg_set_nonblock_state,
   vg_alive,
   vg_focus,
   vg_suppress_screensaver,
   vg_has_windowed,
   vg_set_shader,
   vg_free,
   "vg",
   NULL, /* set_viewport */
   vg_set_rotation,
   vg_viewport_info,
   vg_read_viewport,
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
  NULL, /* overlay_interface */
#endif
  vg_get_poke_interface
};
