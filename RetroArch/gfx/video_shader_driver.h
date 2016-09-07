/*  RetroArch - A frontend for libretro.
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

#ifndef VIDEO_SHADER_DRIVER_H__
#define VIDEO_SHADER_DRIVER_H__

#include <boolean.h>
#include <retro_common_api.h>

#include <gfx/math/matrix_4x4.h>

#include "video_coord_array.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL)
#ifndef HAVE_SHADER_MANAGER
#define HAVE_SHADER_MANAGER
#endif

#include "video_shader_parse.h"

#define VIDEO_SHADER_STOCK_BLEND (GFX_MAX_SHADERS - 1)
#define VIDEO_SHADER_MENU        (GFX_MAX_SHADERS - 2)
#define VIDEO_SHADER_MENU_SEC    (GFX_MAX_SHADERS - 3)

#endif

#if defined(_XBOX360)
#define DEFAULT_SHADER_TYPE RARCH_SHADER_HLSL
#elif defined(__PSL1GHT__) || defined(HAVE_OPENGLES2) || defined(HAVE_GLSL)
#define DEFAULT_SHADER_TYPE RARCH_SHADER_GLSL
#elif defined(__CELLOS_LV2__) || defined(HAVE_CG)
#define DEFAULT_SHADER_TYPE RARCH_SHADER_CG
#else
#define DEFAULT_SHADER_TYPE RARCH_SHADER_NONE
#endif

#include "video_context_driver.h"

RETRO_BEGIN_DECLS

enum shader_uniform_type
{
   UNIFORM_1F = 0,
   UNIFORM_2F,
   UNIFORM_3F,
   UNIFORM_4F,
   UNIFORM_1FV,
   UNIFORM_2FV,
   UNIFORM_3FV,
   UNIFORM_4FV,
   UNIFORM_1I
};

enum shader_program_type
{
   SHADER_PROGRAM_VERTEX = 0,
   SHADER_PROGRAM_FRAGMENT,
   SHADER_PROGRAM_COMBINED
};

struct shader_program_info
{
   void *data;
   const char *vertex;
   const char *fragment;
   const char *combined;
   unsigned idx;
   bool is_file;
};

struct uniform_info
{
   unsigned type; /* shader uniform type */
   bool enabled;

   int32_t location;
   int32_t count;

   struct
   {
      enum shader_program_type type;
      const char *ident;
      uint32_t idx;
      bool add_prefix;
      bool enable;
   } lookup;

   struct
   {
      struct
      {
         intptr_t v0;
         intptr_t v1;
         intptr_t v2;
         intptr_t v3;
      } integer;

      intptr_t *integerv;

      struct
      {
         uintptr_t v0;
         uintptr_t v1;
         uintptr_t v2;
         uintptr_t v3;
      } unsigned_integer;

      uintptr_t *unsigned_integerv;

      struct
      {
         float v0;
         float v1;
         float v2;
         float v3;
      } f;

      float *floatv;
   } result;
};

typedef struct shader_backend
{
   void *(*init)(void *data, const char *path);
   void (*deinit)(void *data);
   void (*set_params)(void *data, void *shader_data,
         unsigned width, unsigned height, 
         unsigned tex_width, unsigned tex_height, 
         unsigned out_width, unsigned out_height,
         unsigned frame_counter,
         const void *info, 
         const void *prev_info,
         const void *feedback_info,
         const void *fbo_info, unsigned fbo_info_cnt);
   void (*set_uniform_parameter)(void *data, struct uniform_info *param,
         void *uniform_data);
   bool (*compile_program)(void *data, unsigned idx,
         void *program_data, struct shader_program_info *program_info);

   void (*use)(void *data, void *shader_data, unsigned index, bool set_active);
   unsigned (*num_shaders)(void *data);
   bool (*filter_type)(void *data, unsigned index, bool *smooth);
   enum gfx_wrap_type (*wrap_type)(void *data, unsigned index);
   void (*shader_scale)(void *data,
         unsigned index, struct gfx_fbo_scale *scale);
   bool (*set_coords)(void *handle_data,
         void *shader_data, const struct video_coords *coords);
   bool (*set_mvp)(void *data, void *shader_data,
         const math_matrix_4x4 *mat);
   unsigned (*get_prev_textures)(void *data);
   bool (*get_feedback_pass)(void *data, unsigned *pass);
   bool (*mipmap_input)(void *data, unsigned index);

   struct video_shader *(*get_current_shader)(void *data);

   enum rarch_shader_type type;

   /* Human readable string. */
   const char *ident;
} shader_backend_t;

typedef struct video_shader_ctx_init
{
   enum rarch_shader_type shader_type;
   const shader_backend_t *shader;
   struct
   {
      bool core_context_enabled;
   } gl;
   void *data;
   const char *path;
} video_shader_ctx_init_t;

typedef struct video_shader_ctx_params
{
   void *data;
   unsigned width;
   unsigned height;
   unsigned tex_width;
   unsigned tex_height;
   unsigned out_width;
   unsigned out_height;
   unsigned frame_counter;
   const void *info;
   const void *prev_info;
   const void *feedback_info;
   const void *fbo_info;
   unsigned fbo_info_cnt;
} video_shader_ctx_params_t;

typedef struct video_shader_ctx_coords
{
   void *handle_data;
   const void *data;
} video_shader_ctx_coords_t;

typedef struct video_shader_ctx_scale
{
   unsigned idx;
   struct gfx_fbo_scale *scale;
} video_shader_ctx_scale_t;

typedef struct video_shader_ctx_info
{
   bool set_active;
   unsigned num;
   unsigned idx;
   void *data;
} video_shader_ctx_info_t;

typedef struct video_shader_ctx_mvp
{
   void *data;
   const math_matrix_4x4 *matrix;
} video_shader_ctx_mvp_t;

typedef struct video_shader_ctx_filter
{
   unsigned index;
   bool *smooth;
} video_shader_ctx_filter_t;

typedef struct video_shader_ctx_wrap
{
   unsigned idx;
   enum gfx_wrap_type type;
} video_shader_ctx_wrap_t;

typedef struct video_shader_ctx
{
   struct video_shader *data;
} video_shader_ctx_t;

typedef struct video_shader_ctx_ident
{
   const char *ident;
} video_shader_ctx_ident_t;

typedef struct video_shader_ctx_texture
{
   unsigned id;
} video_shader_ctx_texture_t;

bool video_shader_driver_get_prev_textures(video_shader_ctx_texture_t *texture);

bool video_shader_driver_get_ident(video_shader_ctx_ident_t *ident);

bool video_shader_driver_get_current_shader(video_shader_ctx_t *shader);

bool video_shader_driver_direct_get_current_shader(video_shader_ctx_t *shader);

bool video_shader_driver_deinit(void);

bool video_shader_driver_set_parameter(struct uniform_info *param);

bool video_shader_driver_set_parameters(video_shader_ctx_params_t *params);

bool video_shader_driver_init_first(void);

bool video_shader_driver_init(video_shader_ctx_init_t *init);

bool video_shader_driver_get_feedback_pass(unsigned *data);

bool video_shader_driver_mipmap_input(unsigned *index);

bool video_shader_driver_set_coords(video_shader_ctx_coords_t *coords);

bool video_shader_driver_scale(video_shader_ctx_scale_t *scaler);

bool video_shader_driver_info(video_shader_ctx_info_t *shader_info);

bool video_shader_driver_set_mvp(video_shader_ctx_mvp_t *mvp);

bool video_shader_driver_filter_type(video_shader_ctx_filter_t *filter);

bool video_shader_driver_compile_program(struct shader_program_info *program_info);

bool video_shader_driver_use(video_shader_ctx_info_t *shader_info);

bool video_shader_driver_wrap_type(video_shader_ctx_wrap_t *wrap);

extern const shader_backend_t gl_glsl_backend;
extern const shader_backend_t hlsl_backend;
extern const shader_backend_t gl_cg_backend;
extern const shader_backend_t shader_null_backend;

RETRO_END_DECLS

#endif
