/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *
 */

#pragma once

#include "cogl/cogl-bitmap-private.h"
#include "cogl/cogl-pipeline-private.h"
#include "cogl/cogl-spans.h"
#include "cogl/cogl-meta-texture.h"
#include "cogl/cogl-framebuffer.h"
#include "cogl/cogl-texture-2d.h"
#include "cogl/cogl-texture-driver.h"


/* Encodes three possibiloities result of transforming a quad */
typedef enum
{
  /* quad doesn't cross the boundaries of a texture */
  COGL_TRANSFORM_NO_REPEAT,
  /* quad crosses boundaries, hardware wrap mode can handle */
  COGL_TRANSFORM_HARDWARE_REPEAT,
  /* quad crosses boundaries, needs software fallback;
   * for a sliced texture, this might not actually involve
   * repeating, just a quad crossing multiple slices */
  COGL_TRANSFORM_SOFTWARE_REPEAT,
} CoglTransformResult;

/* Flags given to the pre_paint method */
typedef enum
{
  /* The texture is going to be used with filters that require
     mipmapping. This gives the texture the opportunity to
     automatically update the mipmap tree */
  COGL_TEXTURE_NEEDS_MIPMAP = 1
} CoglTexturePrePaintFlags;


typedef enum _CoglTextureSourceType {
  COGL_TEXTURE_SOURCE_TYPE_SIZE = 1,
  COGL_TEXTURE_SOURCE_TYPE_BITMAP,
  COGL_TEXTURE_SOURCE_TYPE_EGL_IMAGE,
  COGL_TEXTURE_SOURCE_TYPE_EGL_IMAGE_EXTERNAL
} CoglTextureSourceType;

typedef struct _CoglTextureLoader
{
  CoglTextureSourceType src_type;
  union {
    struct {
      int width;
      int height;
      int depth; /* for 3d textures */
      CoglPixelFormat format;
    } sized;
    struct {
      CoglBitmap *bitmap;
      int height; /* for 3d textures */
      int depth; /* for 3d textures */
    } bitmap;
#if defined (HAVE_EGL) && defined (EGL_KHR_image_base)
    struct {
      EGLImageKHR image;
      int width;
      int height;
      CoglPixelFormat format;
      CoglEglImageFlags flags;
    } egl_image;
#endif
#if defined (HAVE_EGL)
    struct {
      int width;
      int height;
      CoglTexture2DEGLImageExternalAlloc alloc;
      CoglPixelFormat format;
    } egl_image_external;
#endif
    struct {
      int width;
      int height;
      unsigned int gl_handle;
      CoglPixelFormat format;
    } gl_foreign;
  } src;
} CoglTextureLoader;


struct _CoglTextureClass
{
  GObjectClass parent_class;
  gboolean (* allocate) (CoglTexture *tex,
                         GError     **error);

  /* This should update the specified sub region of the texture with a
     sub region of the given bitmap. The bitmap is not converted
     before being set so the caller is expected to have called
     _cogl_bitmap_convert_for_upload with a suitable internal_format
     before passing here */
  gboolean (* set_region) (CoglTexture *tex,
                           int          src_x,
                           int          src_y,
                           int          dst_x,
                           int          dst_y,
                           int          dst_width,
                           int          dst_height,
                           int          level,
                           CoglBitmap  *bitmap,
                           GError     **error);

  gboolean (* is_get_data_supported) (CoglTexture *texture);

  /* This should copy the image data of the texture into @data. The
     requested format will have been first passed through
     TextureDriverClass.find_best_gl_get_data_format so it should
     always be a format that is valid for GL (ie, no conversion should
     be necessary). */
  gboolean (* get_data) (CoglTexture    *tex,
                         CoglPixelFormat format,
                         int             rowstride,
                         uint8_t        *data);

  void (* foreach_sub_texture_in_region) (CoglTexture                *tex,
                                          float                       virtual_tx_1,
                                          float                       virtual_ty_1,
                                          float                       virtual_tx_2,
                                          float                       virtual_ty_2,
                                          CoglTextureForeachCallback  callback,
                                          void                       *user_data);

  gboolean (* is_sliced) (CoglTexture *tex);

  gboolean (* can_hardware_repeat) (CoglTexture *tex);

  void (* transform_coords_to_gl) (CoglTexture *tex,
                                   float       *s,
                                   float       *t);
  CoglTransformResult (* transform_quad_coords_to_gl) (CoglTexture *tex,
                                                       float       *coords);

  gboolean (* get_gl_texture) (CoglTexture *tex,
                               GLuint      *out_gl_handle,
                               GLenum      *out_gl_target);

  /* OpenGL driver specific virtual function */
  void (* gl_flush_legacy_texobj_filters) (CoglTexture *tex,
                                           GLenum       min_filter,
                                           GLenum       mag_filter);

  void (* pre_paint) (CoglTexture             *tex,
                      CoglTexturePrePaintFlags flags);
  void (* ensure_non_quad_rendering) (CoglTexture *tex);

  /* OpenGL driver specific virtual function */
  void (* gl_flush_legacy_texobj_wrap_modes) (CoglTexture *tex,
                                              GLenum       wrap_mode_s,
                                              GLenum       wrap_mode_t);

  CoglPixelFormat (* get_format) (CoglTexture *tex);
  GLenum (* get_gl_format) (CoglTexture *tex);
};

gboolean
_cogl_texture_can_hardware_repeat (CoglTexture *texture);

void
_cogl_texture_pre_paint (CoglTexture *texture, CoglTexturePrePaintFlags flags);

/*
 * This determines a CoglPixelFormat according to texture::components
 * and texture::premultiplied (i.e. the user required components and
 * whether the texture should be considered premultiplied)
 *
 * A reference/source format can be given (or COGL_PIXEL_FORMAT_ANY)
 * and wherever possible this function tries to simply return the
 * given source format if its compatible with the required components.
 *
 * Texture backends can call this when allocating a texture to know
 * how to convert a source image in preparation for uploading.
 */
CoglPixelFormat
_cogl_texture_determine_internal_format (CoglTexture *texture,
                                         CoglPixelFormat src_format);

/* This is called by texture backends when they have successfully
 * allocated a texture.
 *
 * Most texture backends currently track the internal layout of
 * textures using a CoglPixelFormat which will be finalized when a
 * texture is allocated. At this point we need to update
 * texture::components and texture::premultiplied according to the
 * determined layout.
 *
 * XXX: Going forward we should probably aim to stop using
 * CoglPixelFormat at all for tracking the internal layout of
 * textures.
 */
void
_cogl_texture_set_internal_format (CoglTexture *texture,
                                   CoglPixelFormat internal_format);

void
_cogl_texture_associate_framebuffer (CoglTexture *texture,
                                     CoglFramebuffer *framebuffer);

const GList *
_cogl_texture_get_associated_framebuffers (CoglTexture *texture);

void
_cogl_texture_flush_journal_rendering (CoglTexture *texture);

void
_cogl_texture_spans_foreach_in_region (CoglSpan                    *x_spans,
                                       int                          n_x_spans,
                                       CoglSpan                    *y_spans,
                                       int                          n_y_spans,
                                       CoglTexture                **textures,
                                       float                       *virtual_coords,
                                       float                        x_normalize_factor,
                                       float                        y_normalize_factor,
                                       CoglPipelineWrapMode         wrap_x,
                                       CoglPipelineWrapMode         wrap_y,
                                       CoglTextureForeachCallback   callback,
                                       void                        *user_data);

COGL_EXPORT gboolean
_cogl_texture_set_region (CoglTexture *texture,
                          int width,
                          int height,
                          CoglPixelFormat format,
                          int rowstride,
                          const uint8_t *data,
                          int dst_x,
                          int dst_y,
                          int level,
                          GError **error);

gboolean
_cogl_texture_set_region_from_bitmap (CoglTexture *texture,
                                      int src_x,
                                      int src_y,
                                      int width,
                                      int height,
                                      CoglBitmap *bmp,
                                      int dst_x,
                                      int dst_y,
                                      int level,
                                      GError **error);

gboolean
_cogl_texture_needs_premult_conversion (CoglPixelFormat src_format,
                                        CoglPixelFormat dst_format);

int
_cogl_texture_get_n_levels (CoglTexture *texture);

void
cogl_texture_set_max_level (CoglTexture *texture,
                            int          max_level);

void
_cogl_texture_get_level_size (CoglTexture *texture,
                              int level,
                              int *width,
                              int *height,
                              int *depth);

void
_cogl_texture_set_allocated (CoglTexture *texture,
                             CoglPixelFormat internal_format,
                             int width,
                             int height);

CoglTextureLoader *
cogl_texture_loader_new (CoglTextureSourceType type);

void
_cogl_texture_copy_internal_format (CoglTexture *src,
                                    CoglTexture *dest);

CoglTextureLoader *
cogl_texture_get_loader (CoglTexture *texture);

int
cogl_texture_get_max_level_set (CoglTexture *texture);

void
cogl_texture_set_max_level_set (CoglTexture *texture,
                                int          max_level_set);

gboolean cogl_texture_is_allocated (CoglTexture *texture);

CoglTextureDriver * cogl_texture_get_driver (CoglTexture *texture);
