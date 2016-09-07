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

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

#include <boolean.h>

#include "gl_capabilities.h"
#include "../video_driver.h"

static bool gl_core_context       = false;

bool gl_query_core_context_in_use(void)
{
   return gl_core_context;
}

void gl_query_core_context_set(bool set)
{
   gl_core_context     =  set;
}

void gl_query_core_context_unset(void)
{
   gl_core_context = false;
}

static bool gl_query_extension(const char *ext)
{
   bool ret = false;

   if (gl_query_core_context_in_use())
   {
#ifdef GL_NUM_EXTENSIONS
      GLint i;
      GLint exts = 0;
      glGetIntegerv(GL_NUM_EXTENSIONS, &exts);
      for (i = 0; i < exts; i++)
      {
         const char *str = (const char*)glGetStringi(GL_EXTENSIONS, i);
         if (str && strstr(str, ext))
         {
            ret = true;
            break;
         }
      }
#endif
   }
   else
   {
      const char *str = (const char*)glGetString(GL_EXTENSIONS);
      ret = str && strstr(str, ext);
   }

   return ret;
}

bool gl_check_error(char *error_string)
{
   int error = glGetError();
   switch (error)
   {
      case GL_INVALID_ENUM:
         error_string = strdup("GL: Invalid enum.");
         break;
      case GL_INVALID_VALUE:
         error_string = strdup("GL: Invalid value.");
         break;
      case GL_INVALID_OPERATION:
         error_string = strdup("GL: Invalid operation.");
         break;
      case GL_OUT_OF_MEMORY:
         error_string = strdup("GL: Out of memory.");
         break;
      case GL_NO_ERROR:
         return true;
      default:
         error_string = strdup("Non specified GL error.");
         break;
   }
    
   (void)error_string;

   return false;
}

bool gl_check_capability(enum gl_capability_enum enum_idx)
{
   unsigned major       = 0;
   unsigned minor       = 0;
   const char *vendor   = (const char*)glGetString(GL_VENDOR);
   const char *renderer = (const char*)glGetString(GL_RENDERER);
   const char *version  = (const char*)glGetString(GL_VERSION);
#ifdef HAVE_OPENGLES
   if (version && sscanf(version, "OpenGL ES %u.%u", &major, &minor) != 2)
#else
   if (version && sscanf(version, "%u.%u", &major, &minor) != 2)
#endif
      major = minor = 0;
    
   (void)vendor;

   switch (enum_idx)
   {
      case GL_CAPS_EGLIMAGE:
#if defined(HAVE_EGL) && defined(HAVE_OPENGLES)
         if (glEGLImageTargetTexture2DOES != NULL)
            return true;
#endif
         break;
      case GL_CAPS_SYNC:
#ifdef HAVE_OPENGLES
         if (major >= 3)
            return true;
#else
         if (gl_query_extension("ARB_sync") &&
               glFenceSync && glDeleteSync && glClientWaitSync)
            return true;
#endif
         break;
      case GL_CAPS_MIPMAP:
         {
            static bool extension_queried = false;
            static bool extension         = false;

            if (!extension_queried)
            {
               extension         = gl_query_extension("ARB_framebuffer_object");
               extension_queried = true;
            }

            if (extension)
               return true;
         }
         break;
      case GL_CAPS_VAO:
#ifndef HAVE_OPENGLES
         if (!gl_query_core_context_in_use() && !gl_query_extension("ARB_vertex_array_object"))
            return false;

         if (glGenVertexArrays && glBindVertexArray && glDeleteVertexArrays)
            return true;
#endif
         break;
      case GL_CAPS_FBO:
#if defined(HAVE_PSGL) || defined(HAVE_OPENGLES2) || defined(HAVE_OPENGLES3) || defined(HAVE_OPENGLES_3_1) || defined(HAVE_OPENGLES_3_2)
         return true;
#elif defined(HAVE_FBO)
         if (!gl_query_core_context_in_use() && !gl_query_extension("ARB_framebuffer_object")
               && !gl_query_extension("EXT_framebuffer_object"))
            return false;

         if (glGenFramebuffers 
               && glBindFramebuffer 
               && glFramebufferTexture2D 
               && glCheckFramebufferStatus 
               && glDeleteFramebuffers 
               && glGenRenderbuffers 
               && glBindRenderbuffer 
               && glFramebufferRenderbuffer 
               && glRenderbufferStorage 
               && glDeleteRenderbuffers)
            return true;
         break;
#else
         break;
#endif
      case GL_CAPS_ARGB8:
#ifdef HAVE_OPENGLES
         if (gl_query_extension("OES_rgb8_rgba8")
               || gl_query_extension("ARM_argb8"))
            return true;
#else
         /* TODO/FIXME - implement this for non-GLES? */
#endif
         break;
      case GL_CAPS_DEBUG:
         if (gl_query_extension("KHR_debug"))
            return true;
#ifndef HAVE_OPENGLES
         if (gl_query_extension("ARB_debug_output"))
            return true;
#endif
         break;
      case GL_CAPS_PACKED_DEPTH_STENCIL:
         {
            struct retro_hw_render_callback *hwr =
               video_driver_get_hw_context();
            if (major >= 3)
               return true;
            if (hwr->stencil 
                  && !gl_query_extension("OES_packed_depth_stencil")
                  && !gl_query_extension("EXT_packed_depth_stencil"))
               return false;
         }
         return true;
      case GL_CAPS_ES2_COMPAT:
#ifndef HAVE_OPENGLES
         /* ATI card detected, skipping check for GL_RGB565 support... */
         if (vendor && renderer && (strstr(vendor, "ATI") || strstr(renderer, "ATI")))
            return false;

         if (gl_query_extension("ARB_ES2_compatibility"))
            return true;
#endif
         break;
      case GL_CAPS_UNPACK_ROW_LENGTH:
#ifdef HAVE_OPENGLES
         if (major >= 3)
            return true;

         /* Extension GL_EXT_unpack_subimage, can copy textures faster
          * than using UNPACK_ROW_LENGTH */
         if (gl_query_extension("GL_EXT_unpack_subimage"))
            return true;
#endif
         break;
      case GL_CAPS_FULL_NPOT_SUPPORT:
         if (major >= 3)
            return true;
#ifdef HAVE_OPENGLES
         if (gl_query_extension("ARB_texture_non_power_of_two") ||
               gl_query_extension("OES_texture_npot"))
            return true;
#else
         {
            GLint max_texture_size = 0;
            GLint max_native_instr = 0;
            /* try to detect actual npot support. might fail for older cards. */
            bool  arb_npot         = gl_query_extension("ARB_texture_non_power_of_two");
            bool  arb_frag_program = gl_query_extension("ARB_fragment_program");

            glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);

#ifdef GL_MAX_PROGRAM_NATIVE_INSTRUCTIONS_ARB
            if (arb_frag_program && glGetProgramivARB)
               glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB,
                     GL_MAX_PROGRAM_NATIVE_INSTRUCTIONS_ARB, &max_native_instr);
#endif

            if (arb_npot && arb_frag_program &&
                  (max_texture_size >= 8192) && (max_native_instr >= 4096))
               return true;
         }
#endif
         break;
      case GL_CAPS_SRGB_FBO_ES3:
#ifdef HAVE_OPENGLES
         if (major >= 3)
            return true;
#else
         break;
#endif
      case GL_CAPS_SRGB_FBO:
#if defined(HAVE_OPENGLES)
         if (major >= 3 || gl_query_extension("EXT_sRGB"))
            return true;
#elif defined(HAVE_FBO)
         if (gl_query_core_context_in_use() || 
               (gl_query_extension("EXT_texture_sRGB")
                && gl_query_extension("ARB_framebuffer_sRGB")))
            return true;
#endif
         break;
      case GL_CAPS_FP_FBO:
         /* GLES - No extensions for float FBO currently. */
#ifndef HAVE_OPENGLES
#ifdef HAVE_FBO
         /* Float FBO is core in 3.2. */
         if (gl_query_core_context_in_use() || gl_query_extension("ARB_texture_float"))
            return true;
#endif
#endif
         break;
      case GL_CAPS_BGRA8888:
#ifdef HAVE_OPENGLES
         /* There are both APPLE and EXT variants. */
         /* Videocore hardware supports BGRA8888 extension, but
          * should be purposefully avoided. */
         if (gl_query_extension("BGRA8888") && !strstr(renderer, "VideoCore"))
            return true;
#else
         /* TODO/FIXME - implement this for non-GLES? */
#endif
         break;
      case GL_CAPS_NONE:
      default:
         break;
   }

   return false;
}
