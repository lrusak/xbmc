/*
 *  Copyright (C) 2007-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "VaapiEGL.h"

#include "cores/VideoPlayer/DVDCodecs/Video/VAAPI.h"
#include "utils/EGLUtils.h"
#include "utils/log.h"

#include <drm_fourcc.h>
#include <va/va_drmcommon.h>

#define HAVE_VAEXPORTSURFACHEHANDLE VA_CHECK_VERSION(1, 1, 0)

using namespace VAAPI;

CVaapi1Texture::CVaapi1Texture(EGLDisplay eglDisplay) : CVaapiTexture(eglDisplay)
{
  m_glSurface.eglImage = std::make_unique<CEGLImage>(eglDisplay);
  m_glSurface.eglImageY = std::make_unique<CEGLImage>(eglDisplay);
  m_glSurface.eglImageVU = std::make_unique<CEGLImage>(eglDisplay);
}

bool CVaapi1Texture::Map(CVaapiRenderPicture *pic)
{
  VAStatus status;

  if (m_vaapiPic)
    return true;

  vaSyncSurface(pic->vadsp, pic->procPic.videoSurface);

  status = vaDeriveImage(pic->vadsp, pic->procPic.videoSurface, &m_glSurface.vaImage);
  if (status != VA_STATUS_SUCCESS)
  {
    CLog::Log(LOGERROR, "CVaapiTexture::{} - Error: {}({})", __FUNCTION__, vaErrorStr(status),
              status);
    return false;
  }
  memset(&m_glSurface.vBufInfo, 0, sizeof(m_glSurface.vBufInfo));
  m_glSurface.vBufInfo.mem_type = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME;
  status = vaAcquireBufferHandle(pic->vadsp, m_glSurface.vaImage.buf, &m_glSurface.vBufInfo);
  if (status != VA_STATUS_SUCCESS)
  {
    CLog::Log(LOGERROR, "CVaapiTexture::{} - Error: {}({})", __FUNCTION__, vaErrorStr(status),
              status);
    return false;
  }

  m_texWidth = m_glSurface.vaImage.width;
  m_texHeight = m_glSurface.vaImage.height;

  switch (m_glSurface.vaImage.format.fourcc)
  {
    case VA_FOURCC('N','V','1','2'):
    {
      std::array<CEGLImage::EglPlane, CEGLImage::MAX_NUM_PLANES> planes;

      planes[0].fd = (intptr_t)m_glSurface.vBufInfo.handle;
      planes[0].offset = m_glSurface.vaImage.offsets[0];
      planes[0].pitch = m_glSurface.vaImage.pitches[0];

      CEGLImage::EglAttrs attribs;

      attribs.width = m_glSurface.vaImage.width;
      attribs.height = m_glSurface.vaImage.height;
      attribs.format = fourcc_code('R', '8', ' ', ' ');
      attribs.planes = planes;

      if (!m_glSurface.eglImageY->CreateImage(attribs))
        return false;

      planes[0].fd = (intptr_t)m_glSurface.vBufInfo.handle;
      planes[0].offset = m_glSurface.vaImage.offsets[1];
      planes[0].pitch = m_glSurface.vaImage.pitches[1];

      attribs.width = (m_glSurface.vaImage.width + 1) >> 1;
      attribs.height = (m_glSurface.vaImage.height + 1) >> 1;
      attribs.format = fourcc_code('G', 'R', '8', '8');
      attribs.planes = planes;

      if (!m_glSurface.eglImageVU->CreateImage(attribs))
        return false;

      glGenTextures(1, &m_textureY);
      glBindTexture(GL_TEXTURE_2D, m_textureY);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      m_glSurface.eglImageY->UploadImage(GL_TEXTURE_2D);
      m_glSurface.eglImageY->DestroyImage();

      glGenTextures(1, &m_textureVU);
      glBindTexture(GL_TEXTURE_2D, m_textureVU);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      m_glSurface.eglImageVU->UploadImage(GL_TEXTURE_2D);
      m_glSurface.eglImageVU->DestroyImage();

      glBindTexture(GL_TEXTURE_2D, 0);

      break;
    }
    case VA_FOURCC('P','0','1','0'):
    {
      std::array<CEGLImage::EglPlane, CEGLImage::MAX_NUM_PLANES> planes;

      planes[0].fd = (intptr_t)m_glSurface.vBufInfo.handle;
      planes[0].offset = m_glSurface.vaImage.offsets[0];
      planes[0].pitch = m_glSurface.vaImage.pitches[0];

      CEGLImage::EglAttrs attribs;

      attribs.width = m_glSurface.vaImage.width;
      attribs.height = m_glSurface.vaImage.height;
      attribs.format = fourcc_code('R', '1', '6', ' ');
      attribs.planes = planes;

      if (!m_glSurface.eglImageY->CreateImage(attribs))
        return false;

      planes[0].fd = (intptr_t)m_glSurface.vBufInfo.handle;
      planes[0].offset = m_glSurface.vaImage.offsets[1];
      planes[0].pitch = m_glSurface.vaImage.pitches[1];

      attribs.width = (m_glSurface.vaImage.width + 1) >> 1;
      attribs.height = (m_glSurface.vaImage.height + 1) >> 1;
      attribs.format = fourcc_code('G', 'R', '3', '2');
      attribs.planes = planes;

      if (!m_glSurface.eglImageVU->CreateImage(attribs))
        return false;

      glGenTextures(1, &m_textureY);
      glBindTexture(GL_TEXTURE_2D, m_textureY);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      m_glSurface.eglImageY->UploadImage(GL_TEXTURE_2D);
      m_glSurface.eglImageY->DestroyImage();

      glGenTextures(1, &m_textureVU);
      glBindTexture(GL_TEXTURE_2D, m_textureVU);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      m_glSurface.eglImageVU->UploadImage(GL_TEXTURE_2D);
      m_glSurface.eglImageVU->DestroyImage();

      glBindTexture(GL_TEXTURE_2D, 0);

      break;
    }
    default:
      return false;
  }

  m_vaapiPic = pic;
  m_vaapiPic->Acquire();
  return true;
}

void CVaapi1Texture::Unmap()
{
  if (!m_vaapiPic)
    return;

  if (m_glSurface.vaImage.image_id == VA_INVALID_ID)
    return;

  VAStatus status;
  status = vaReleaseBufferHandle(m_vaapiPic->vadsp, m_glSurface.vaImage.buf);
  if (status != VA_STATUS_SUCCESS)
  {
    CLog::Log(LOGERROR, "VAAPI::{} - Error: {}({})", __FUNCTION__, vaErrorStr(status), status);
  }

  status = vaDestroyImage(m_vaapiPic->vadsp, m_glSurface.vaImage.image_id);
  if (status != VA_STATUS_SUCCESS)
  {
    CLog::Log(LOGERROR, "VAAPI::{} - Error: {}({})", __FUNCTION__, vaErrorStr(status), status);
  }

  m_glSurface.vaImage.image_id = VA_INVALID_ID;

  glDeleteTextures(1, &m_textureY);
  glDeleteTextures(1, &m_textureVU);

  m_vaapiPic->Release();
  m_vaapiPic = nullptr;
}

GLuint CVaapi1Texture::GetTextureY()
{
  return m_textureY;
}

GLuint CVaapi1Texture::GetTextureVU()
{
  return m_textureVU;
}

CSizeInt CVaapi1Texture::GetTextureSize()
{
  return {m_texWidth, m_texHeight};
}

void CVaapi1Texture::TestInterop(VADisplay vaDpy, EGLDisplay eglDisplay, bool &general, bool &deepColor)
{
  general = false;
  deepColor = false;

  int width = 1920;
  int height = 1080;

  // create surfaces
  VASurfaceID surface;
  VAStatus status;
  VAImage image;
  VABufferInfo bufferInfo;

  if (vaCreateSurfaces(vaDpy,  VA_RT_FORMAT_YUV420,
                       width, height,
                       &surface, 1, NULL, 0) != VA_STATUS_SUCCESS)
  {
    return;
  }

  // check interop

  status = vaDeriveImage(vaDpy, surface, &image);
  if (status == VA_STATUS_SUCCESS)
  {
    memset(&bufferInfo, 0, sizeof(bufferInfo));
    bufferInfo.mem_type = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME;
    status = vaAcquireBufferHandle(vaDpy, image.buf, &bufferInfo);
    if (status == VA_STATUS_SUCCESS)
    {
      std::array<CEGLImage::EglPlane, CEGLImage::MAX_NUM_PLANES> planes;

      planes[0].fd = bufferInfo.handle;
      planes[0].offset = image.offsets[0];
      planes[0].pitch = image.pitches[0];

      CEGLImage::EglAttrs attribs;

      attribs.width = image.width;
      attribs.height = image.height;
      attribs.format = DRM_FORMAT_R8;
      attribs.planes = planes;

      CEGLImage eglImage(eglDisplay);
      if (eglImage.CreateImage(attribs))
      {
        eglImage.DestroyImage();
        general = true;
      }
    }
    vaDestroyImage(vaDpy, image.image_id);
  }
  vaDestroySurfaces(vaDpy, &surface, 1);

  if (general)
  {
    deepColor = TestInteropDeepColor(vaDpy, eglDisplay);
  }
}

bool CVaapi1Texture::TestInteropDeepColor(VADisplay vaDpy, EGLDisplay eglDisplay)
{
  bool ret = false;

  int width = 1920;
  int height = 1080;

  // create surfaces
  VASurfaceID surface;
  VAStatus status;
  VAImage image;
  VABufferInfo bufferInfo;

  VASurfaceAttrib attribs = {};
  attribs.flags = VA_SURFACE_ATTRIB_SETTABLE;
  attribs.type = VASurfaceAttribPixelFormat;
  attribs.value.type = VAGenericValueTypeInteger;
  attribs.value.value.i = VA_FOURCC_P010;

  if (vaCreateSurfaces(vaDpy,  VA_RT_FORMAT_YUV420_10BPP,
                       width, height,
                       &surface, 1, &attribs, 1) != VA_STATUS_SUCCESS)
  {
    return false;
  }

  // check interop
  status = vaDeriveImage(vaDpy, surface, &image);
  if (status == VA_STATUS_SUCCESS)
  {
    memset(&bufferInfo, 0, sizeof(bufferInfo));
    bufferInfo.mem_type = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME;
    status = vaAcquireBufferHandle(vaDpy, image.buf, &bufferInfo);
    if (status == VA_STATUS_SUCCESS)
    {
      std::array<CEGLImage::EglPlane, CEGLImage::MAX_NUM_PLANES> planes;

      planes[0].fd = bufferInfo.handle;
      planes[0].offset = image.offsets[1];
      planes[0].pitch = image.pitches[1];

      CEGLImage::EglAttrs attribs;

      attribs.width = (image.width + 1) >> 1;
      attribs.height = (image.height + 1) >> 1;
      attribs.format = DRM_FORMAT_GR1616;
      attribs.planes = planes;

      CEGLImage eglImage(eglDisplay);

      if (eglImage.CreateImage(attribs))
      {
        eglImage.DestroyImage();
        ret = true;
      }

    }
    vaDestroyImage(vaDpy, image.image_id);
  }

  vaDestroySurfaces(vaDpy, &surface, 1);

  return ret;
}

CVaapi2Texture::CVaapi2Texture(EGLDisplay eglDisplay) : CVaapiTexture(eglDisplay)
{
  m_y.eglImage = std::make_unique<CEGLImage>(eglDisplay);
  m_vu.eglImage = std::make_unique<CEGLImage>(eglDisplay);
  m_hasPlaneModifiers =
      CEGLUtils::HasExtension(eglDisplay, "EGL_EXT_image_dma_buf_import_modifiers");
}

bool CVaapi2Texture::Map(CVaapiRenderPicture* pic)
{
#if HAVE_VAEXPORTSURFACHEHANDLE
  if (m_vaapiPic)
    return true;

  m_vaapiPic = pic;
  m_vaapiPic->Acquire();

  VAStatus status;

  VADRMPRIMESurfaceDescriptor surface;

  status = vaExportSurfaceHandle(pic->vadsp, pic->procPic.videoSurface,
    VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
    VA_EXPORT_SURFACE_READ_ONLY | VA_EXPORT_SURFACE_SEPARATE_LAYERS,
    &surface);

  if (status != VA_STATUS_SUCCESS)
  {
    CLog::Log(LOGWARNING, "CVaapi2Texture::Map: vaExportSurfaceHandle failed - Error: {} ({})",
              vaErrorStr(status), status);
    return false;
  }

  // Remember fds to close them later
  if (surface.num_objects > m_drmFDs.size())
    throw std::logic_error("Too many fds returned by vaExportSurfaceHandle");

  for (uint32_t object = 0; object < surface.num_objects; object++)
  {
    m_drmFDs[object].attach(surface.objects[object].fd);
  }

  status = vaSyncSurface(pic->vadsp, pic->procPic.videoSurface);
  if (status != VA_STATUS_SUCCESS)
  {
    CLog::Log(LOGERROR, "CVaapi2Texture::Map: vaSyncSurface - Error: {} ({})", vaErrorStr(status),
              status);
    return false;
  }

  m_textureSize.Set(pic->DVDPic.iWidth, pic->DVDPic.iHeight);

  for (uint32_t layerNo = 0; layerNo < surface.num_layers; layerNo++)
  {
    int plane = 0;
    auto const& layer = surface.layers[layerNo];
    if (layer.num_planes != 1)
    {
      CLog::Log(LOGDEBUG,
                "CVaapi2Texture::Map: DRM-exported layer has {} planes - only 1 supported",
                layer.num_planes);
      return false;
    }
    auto const& object = surface.objects[layer.object_index[plane]];

    MappedTexture* texture{};
    EGLint width{m_textureSize.Width()};
    EGLint height{m_textureSize.Height()};

    switch (surface.num_layers)
    {
      case 2:
        switch (layerNo)
        {
          case 0:
            texture = &m_y;
            break;
          case 1:
            texture = &m_vu;
            if (surface.fourcc == VA_FOURCC_NV12 || surface.fourcc == VA_FOURCC_P010 || surface.fourcc == VA_FOURCC_P016)
            {
              // Adjust w/h for 4:2:0 subsampling on UV plane
              width = (width + 1) >> 1;
              height = (height + 1) >> 1;
            }
            break;
          default:
            throw std::logic_error("Impossible layer number");
        }
        break;
      default:
        CLog::Log(LOGDEBUG,
                  "CVaapi2Texture::Map: DRM-exported surface {} layers - only 2 supported",
                  surface.num_layers);
        return false;
    }

    std::array<CEGLImage::EglPlane, CEGLImage::MAX_NUM_PLANES> planes;

    planes[0].fd = object.fd;
    planes[0].offset = static_cast<EGLint>(layer.offset[plane]);
    planes[0].pitch = static_cast<EGLint>(layer.pitch[plane]);

    if (m_hasPlaneModifiers)
      planes[0].modifier = object.drm_format_modifier;

    CEGLImage::EglAttrs attribs;

    attribs.width = width;
    attribs.height = height;
    attribs.format = static_cast<EGLint>(layer.drm_format);
    attribs.planes = planes;

    if (!texture->eglImage->CreateImage(attribs))
      return false;

    glGenTextures(1, &texture->glTexture);
    glBindTexture(GL_TEXTURE_2D, texture->glTexture);
    texture->eglImage->UploadImage(GL_TEXTURE_2D);
    texture->eglImage->DestroyImage();
    glBindTexture(GL_TEXTURE_2D, 0);
  }

  return true;
#else
  return false;
#endif
}

void CVaapi2Texture::Unmap()
{
  if (!m_vaapiPic)
    return;

  for (auto texture : {&m_y, &m_vu})
  {
    glDeleteTextures(1, &texture->glTexture);
  }

  for (auto& fd : m_drmFDs)
  {
    fd.reset();
  }

  m_vaapiPic->Release();
  m_vaapiPic = nullptr;
}

GLuint CVaapi2Texture::GetTextureY()
{
  return m_y.glTexture;
}

GLuint CVaapi2Texture::GetTextureVU()
{
  return m_vu.glTexture;
}

CSizeInt CVaapi2Texture::GetTextureSize()
{
  return m_textureSize;
}

bool CVaapi2Texture::TestEsh(VADisplay vaDpy, EGLDisplay eglDisplay, std::uint32_t rtFormat, std::int32_t pixelFormat)
{
#if HAVE_VAEXPORTSURFACHEHANDLE
  int width = 1920;
  int height = 1080;

  // create surfaces
  VASurfaceID surface;
  VAStatus status;

  VASurfaceAttrib attribs = {};
  attribs.flags = VA_SURFACE_ATTRIB_SETTABLE;
  attribs.type = VASurfaceAttribPixelFormat;
  attribs.value.type = VAGenericValueTypeInteger;
  attribs.value.value.i = pixelFormat;

  if (vaCreateSurfaces(vaDpy, rtFormat,
        width, height,
        &surface, 1, &attribs, 1) != VA_STATUS_SUCCESS)
  {
    return false;
  }

  // check interop
  VADRMPRIMESurfaceDescriptor drmPrimeSurface;
  status = vaExportSurfaceHandle(vaDpy, surface,
    VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
    VA_EXPORT_SURFACE_READ_ONLY | VA_EXPORT_SURFACE_SEPARATE_LAYERS,
    &drmPrimeSurface);

  bool result = false;

  if (status == VA_STATUS_SUCCESS)
  {
    auto const& layer = drmPrimeSurface.layers[0];
    auto const& object = drmPrimeSurface.objects[layer.object_index[0]];

    std::array<CEGLImage::EglPlane, CEGLImage::MAX_NUM_PLANES> planes;

    planes[0].fd = object.fd;
    planes[0].offset = layer.offset[0];
    planes[0].pitch = layer.pitch[0];

    CEGLImage::EglAttrs attribs;

    attribs.width = width;
    attribs.height = height;
    attribs.format = drmPrimeSurface.layers[0].drm_format;
    attribs.planes = planes;

    CEGLImage eglImage(eglDisplay);
    if (eglImage.CreateImage(attribs))
    {
      eglImage.DestroyImage();
      result = true;
    }

    for (uint32_t object = 0; object < drmPrimeSurface.num_objects; object++)
    {
      close(drmPrimeSurface.objects[object].fd);
    }
  }

  vaDestroySurfaces(vaDpy, &surface, 1);

  return result;
#else
  return false;
#endif
}

void CVaapi2Texture::TestInterop(VADisplay vaDpy, EGLDisplay eglDisplay, bool& general, bool& deepColor)
{
  general = false;
  deepColor = false;

  general = TestInteropGeneral(vaDpy, eglDisplay);
  if (general)
  {
    deepColor = TestEsh(vaDpy, eglDisplay, VA_RT_FORMAT_YUV420_10BPP, VA_FOURCC_P010);
  }
}

bool CVaapi2Texture::TestInteropGeneral(VADisplay vaDpy, EGLDisplay eglDisplay)
{
  return TestEsh(vaDpy, eglDisplay, VA_RT_FORMAT_YUV420, VA_FOURCC_NV12);
}
