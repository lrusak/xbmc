/*
 *  Copyright (C) 2007-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "VaapiEGL.h"

#include "cores/VideoPlayer/DVDCodecs/Video/VAAPI.h"
#include <va/va_drmcommon.h>
#include <drm_fourcc.h>
#include "utils/log.h"
#include "utils/EGLUtils.h"

#define HAVE_VAEXPORTSURFACHEHANDLE VA_CHECK_VERSION(1, 1, 0)

using namespace VAAPI;

int CVaapiTexture::GetColorSpace(int colorSpace)
{
  switch (colorSpace)
  {
    case AVCOL_SPC_BT2020_CL:
    case AVCOL_SPC_BT2020_NCL:
      return EGL_ITU_REC2020_EXT;
    case AVCOL_SPC_SMPTE170M:
    case AVCOL_SPC_BT470BG:
    case AVCOL_SPC_FCC:
      return EGL_ITU_REC601_EXT;
    case AVCOL_SPC_BT709:
      return EGL_ITU_REC709_EXT;
    case AVCOL_SPC_RESERVED:
    case AVCOL_SPC_UNSPECIFIED:
    default:
      return EGL_ITU_REC601_EXT;
  }
}

int CVaapiTexture::GetColorRange(int colorRange)
{
  switch (colorRange)
  {
    case AVCOL_RANGE_JPEG:
      return EGL_YUV_FULL_RANGE_EXT;
    case AVCOL_RANGE_MPEG:
    default:
      return EGL_YUV_NARROW_RANGE_EXT;
  }
}

CVaapi1Texture::CVaapi1Texture()
{
}

void CVaapi1Texture::Init(EGLDisplay eglDisplay)
{
  m_glSurface.eglImageY.reset(new CEGLImage(eglDisplay));
  m_glSurface.eglImageVU.reset(new CEGLImage(eglDisplay));
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
    CLog::Log(LOGERROR, "CVaapiTexture::%s - Error: %s(%d)", __FUNCTION__, vaErrorStr(status), status);
    return false;
  }
  memset(&m_glSurface.vBufInfo, 0, sizeof(m_glSurface.vBufInfo));
  m_glSurface.vBufInfo.mem_type = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME;
  status = vaAcquireBufferHandle(pic->vadsp, m_glSurface.vaImage.buf, &m_glSurface.vBufInfo);
  if (status != VA_STATUS_SUCCESS)
  {
    CLog::Log(LOGERROR, "CVaapiTexture::%s - Error: %s(%d)", __FUNCTION__, vaErrorStr(status), status);
    return false;
  }

  m_texWidth = m_glSurface.vaImage.width;
  m_texHeight = m_glSurface.vaImage.height;

  switch (m_glSurface.vaImage.format.fourcc)
  {
    case VA_FOURCC('N','V','1','2'):
    {
      m_bits = 8;

      std::array<CEGLImage::EglPlane, CEGLImage::MAX_NUM_PLANES> planes;

      planes[0].fd = static_cast<intptr_t>(m_glSurface.vBufInfo.handle);
      planes[0].offset = m_glSurface.vaImage.offsets[0];
      planes[0].pitch = m_glSurface.vaImage.pitches[0];

      CEGLImage::EglAttrs attribs;

      attribs.width = m_glSurface.vaImage.width;
      attribs.height = m_glSurface.vaImage.height;
      attribs.format = DRM_FORMAT_R8;
      attribs.colorSpace = GetColorSpace(pic->DVDPic.color_space);
      attribs.colorRange = GetColorRange(pic->DVDPic.color_range);
      attribs.planes = planes;

      if (!m_glSurface.eglImageY->CreateImage(attribs))
      {
        return false;
      }

      planes = {};

      planes[0].fd = m_glSurface.vBufInfo.handle;
      planes[0].offset = m_glSurface.vaImage.offsets[1];
      planes[0].pitch = m_glSurface.vaImage.pitches[1];

      attribs = {};

      attribs.width = (m_glSurface.vaImage.width + 1) >> 1;
      attribs.height = (m_glSurface.vaImage.height + 1) >> 1;
      attribs.format = DRM_FORMAT_GR88;
      attribs.colorSpace = GetColorSpace(pic->DVDPic.color_space);
      attribs.colorRange = GetColorRange(pic->DVDPic.color_range);
      attribs.planes = planes;

      if (!m_glSurface.eglImageVU->CreateImage(attribs))
      {
        return false;
      }

      GLint format, type;

      glGenTextures(1, &m_textureY);
      glBindTexture(m_textureTarget, m_textureY);
      glTexParameteri(m_textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(m_textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(m_textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(m_textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      m_glSurface.eglImageY->UploadImage(m_textureTarget);
      glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &format);

      glGenTextures(1, &m_textureVU);
      glBindTexture(m_textureTarget, m_textureVU);
      glTexParameteri(m_textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(m_textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(m_textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(m_textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      m_glSurface.eglImageVU->UploadImage(m_textureTarget);
      glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &format);
      glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &type);

      if (type == GL_UNSIGNED_BYTE)
        m_bits = 8;
      else if (type == GL_UNSIGNED_SHORT)
        m_bits = 16;
      else
      {
        CLog::Log(LOGWARNING, "Did not expect texture type: %d", (int) type);
        m_bits = 8;
      }

      glBindTexture(m_textureTarget, 0);

      break;
    }
    case VA_FOURCC('P','0','1','0'):
    {
      m_bits = 10;

      std::array<CEGLImage::EglPlane, CEGLImage::MAX_NUM_PLANES> planes;

      planes[0].fd = m_glSurface.vBufInfo.handle;
      planes[0].offset = m_glSurface.vaImage.offsets[0];
      planes[0].pitch = m_glSurface.vaImage.pitches[0];

      CEGLImage::EglAttrs attribs;

      attribs.width = m_glSurface.vaImage.width;
      attribs.height = m_glSurface.vaImage.height;
      attribs.format = DRM_FORMAT_R16;
      attribs.colorSpace = GetColorSpace(pic->DVDPic.color_space);
      attribs.colorRange = GetColorRange(pic->DVDPic.color_range);
      attribs.planes = planes;

      if (!m_glSurface.eglImageY->CreateImage(attribs))
      {
        return false;
      }

      planes ={};

      planes[0].fd = m_glSurface.vBufInfo.handle;
      planes[0].offset = m_glSurface.vaImage.offsets[1];
      planes[0].pitch = m_glSurface.vaImage.pitches[1];

      attribs = {};

      attribs.width = (m_glSurface.vaImage.width + 1) >> 1;
      attribs.height = (m_glSurface.vaImage.height + 1) >> 1;
      attribs.format = DRM_FORMAT_GR1616;
      attribs.colorSpace = GetColorSpace(pic->DVDPic.color_space);
      attribs.colorRange = GetColorRange(pic->DVDPic.color_range);
      attribs.planes = planes;

      if (!m_glSurface.eglImageVU->CreateImage(attribs))
      {
        return false;
      }

      GLint format, type;

      glGenTextures(1, &m_textureY);
      glBindTexture(m_textureTarget, m_textureY);
      glTexParameteri(m_textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(m_textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(m_textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(m_textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      m_glSurface.eglImageY->UploadImage(m_textureTarget);
      glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &format);

      glGenTextures(1, &m_textureVU);
      glBindTexture(m_textureTarget, m_textureVU);
      glTexParameteri(m_textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(m_textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(m_textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(m_textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      m_glSurface.eglImageVU->UploadImage(m_textureTarget);
      glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &format);
      glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &type);

      if (type == GL_UNSIGNED_BYTE)
        m_bits = 8;
      else if (type == GL_UNSIGNED_SHORT)
        m_bits = 16;
      else
      {
        CLog::Log(LOGWARNING, "Did not expect texture type: %d", (int) type);
        m_bits = 8;
      }

      glBindTexture(m_textureTarget, 0);

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

  m_glSurface.eglImageY->DestroyImage();
  m_glSurface.eglImageVU->DestroyImage();

  VAStatus status;
  status = vaReleaseBufferHandle(m_vaapiPic->vadsp, m_glSurface.vaImage.buf);
  if (status != VA_STATUS_SUCCESS)
  {
    CLog::Log(LOGERROR, "VAAPI::%s - Error: %s(%d)", __FUNCTION__, vaErrorStr(status), status);
  }

  status = vaDestroyImage(m_vaapiPic->vadsp, m_glSurface.vaImage.image_id);
  if (status != VA_STATUS_SUCCESS)
  {
    CLog::Log(LOGERROR, "VAAPI::%s - Error: %s(%d)", __FUNCTION__, vaErrorStr(status), status);
  }

  m_glSurface.vaImage.image_id = VA_INVALID_ID;

  glDeleteTextures(1, &m_textureY);
  glDeleteTextures(1, &m_textureVU);

  m_vaapiPic->Release();
  m_vaapiPic = nullptr;
}

int CVaapi1Texture::GetBits()
{
  return m_bits;
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
      CEGLImage eglImage(eglDisplay);

      std::array<CEGLImage::EglPlane, CEGLImage::MAX_NUM_PLANES> planes;

      planes[0].fd = bufferInfo.handle;
      planes[0].offset = image.offsets[0];
      planes[0].pitch = image.pitches[0];

      CEGLImage::EglAttrs attribs;

      attribs.width = image.width;
      attribs.height = image.height;
      attribs.format = DRM_FORMAT_R8;
      attribs.planes = planes;

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

  VASurfaceAttrib attribs = { };
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
      CEGLImage eglImage(eglDisplay);

      std::array<CEGLImage::EglPlane, CEGLImage::MAX_NUM_PLANES> planes;

      planes[0].fd = bufferInfo.handle;
      planes[0].offset = image.offsets[1];
      planes[0].pitch = image.pitches[1];

      CEGLImage::EglAttrs attribs;

      attribs.width = (image.width + 1) >> 1;
      attribs.height = (image.height + 1) >> 1;
      attribs.format = DRM_FORMAT_GR1616;
      attribs.planes = planes;

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

void CVaapi2Texture::Init(EGLDisplay eglDisplay)
{
  m_y.eglImage.reset(new CEGLImage(eglDisplay));
  m_vu.eglImage.reset(new CEGLImage(eglDisplay));
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
    CLog::LogFunction(LOGWARNING, "CVaapi2Texture::Map", "vaExportSurfaceHandle failed - Error: %s (%d)", vaErrorStr(status), status);
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
    CLog::LogFunction(LOGERROR, "CVaapi2Texture::Map", "vaSyncSurface - Error: %s (%d)", vaErrorStr(status), status);
    return false;
  }

  m_textureSize.Set(pic->DVDPic.iWidth, pic->DVDPic.iHeight);

  GLint type;
  glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &type);
  if (type == GL_UNSIGNED_BYTE)
    m_bits = 8;
  else if (type == GL_UNSIGNED_SHORT)
    m_bits = 16;
  else
  {
    CLog::Log(LOGWARNING, "Did not expect texture type: %d", static_cast<int> (type));
    m_bits = 8;
  }

  for (uint32_t layerNo = 0; layerNo < surface.num_layers; layerNo++)
  {
    int plane = 0;
    auto const& layer = surface.layers[layerNo];
    if (layer.num_planes != 1)
    {
      CLog::LogFunction(LOGDEBUG, "CVaapi2Texture::Map", "DRM-exported layer has %d planes - only 1 supported", layer.num_planes);
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
        CLog::LogFunction(LOGDEBUG, "CVaapi2Texture::Map", "DRM-exported surface %d layers - only 2 supported", surface.num_layers);
        return false;
    }

    std::array<CEGLImage::EglPlane, CEGLImage::MAX_NUM_PLANES> planes;

    planes[0].fd = object.fd;
    planes[0].offset = layer.offset[plane];
    planes[0].pitch = layer.pitch[plane];
    planes[0].modifier = object.drm_format_modifier;


    CEGLImage::EglAttrs attribs;

    attribs.width = width;
    attribs.height = height;
    attribs.format = layer.drm_format;
    attribs.colorSpace = GetColorSpace(pic->DVDPic.color_space);
    attribs.colorRange = GetColorRange(pic->DVDPic.color_range);
    attribs.planes = planes;

    if (!texture->eglImage->CreateImage(attribs))
    {
      return false;
    }

    glGenTextures(1, &texture->glTexture);
    glBindTexture(m_textureTarget, texture->glTexture);
    texture->eglImage->UploadImage(m_textureTarget);
    glBindTexture(m_textureTarget, 0);
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
    texture->eglImage->DestroyImage();
    glDeleteTextures(1, &texture->glTexture);
  }

  for (auto& fd : m_drmFDs)
  {
    fd.reset();
  }

  m_vaapiPic->Release();
  m_vaapiPic = nullptr;
}

int CVaapi2Texture::GetBits()
{
  return m_bits;
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

    CEGLImage eglImage(eglDisplay);

    std::array<CEGLImage::EglPlane, CEGLImage::MAX_NUM_PLANES> planes;

    planes[0].fd = object.fd;
    planes[0].offset = layer.offset[0];
    planes[0].pitch = layer.pitch[0];

    CEGLImage::EglAttrs attribs;

    attribs.width = width;
    attribs.height = height;
    attribs.format = drmPrimeSurface.layers[0].drm_format;
    attribs.planes = planes;

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
