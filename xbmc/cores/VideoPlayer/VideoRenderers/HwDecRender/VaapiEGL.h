/*
 *  Copyright (C) 2007-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <array>
#include <memory>

#if defined(HAS_GL)
// always define GL_GLEXT_PROTOTYPES before include gl headers
#if !defined(GL_GLEXT_PROTOTYPES)
#define GL_GLEXT_PROTOTYPES
#endif
#include <GL/gl.h>
#elif defined(HAS_GLES)
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#endif

#include "system_egl.h"
#include "utils/EGLImage.h"
#include "utils/Geometry.h"

#include "platform/posix/utils/FileHandle.h"

#include <EGL/eglext.h>
#include <va/va.h>

namespace VAAPI
{

class CVaapiRenderPicture;

class CVaapiTexture
{
public:
  explicit CVaapiTexture(EGLDisplay eglDisplay) : m_eglDisplay(eglDisplay) {}
  virtual ~CVaapiTexture() = default;

  virtual bool Map(CVaapiRenderPicture *pic) = 0;
  virtual void Unmap() = 0;

  virtual GLuint GetTextureY() = 0;
  virtual GLuint GetTextureVU() = 0;
  virtual CSizeInt GetTextureSize() = 0;

protected:
  EGLDisplay m_eglDisplay;
};

class CVaapi1Texture : public CVaapiTexture
{
public:
  explicit CVaapi1Texture(EGLDisplay eglDisplay);
  ~CVaapi1Texture() = default;

  bool Map(CVaapiRenderPicture *pic) override;
  void Unmap() override;

  GLuint GetTextureY() override;
  GLuint GetTextureVU() override;
  CSizeInt GetTextureSize() override;

  static void TestInterop(VADisplay vaDpy, EGLDisplay eglDisplay, bool &general, bool &deepColor);

  GLuint m_texture = 0;
  GLuint m_textureY = 0;
  GLuint m_textureVU = 0;
  int m_texWidth = 0;
  int m_texHeight = 0;

protected:
  static bool TestInteropDeepColor(VADisplay vaDpy, EGLDisplay eglDisplay);

  CVaapiRenderPicture *m_vaapiPic = nullptr;
  struct GLSurface
  {
    VAImage vaImage;
    VABufferInfo vBufInfo;
    std::unique_ptr<CEGLImage> eglImage;
    std::unique_ptr<CEGLImage> eglImageY;
    std::unique_ptr<CEGLImage> eglImageVU;
  } m_glSurface;
};

class CVaapi2Texture : public CVaapiTexture
{
public:
  explicit CVaapi2Texture(EGLDisplay eglDisplay);
  ~CVaapi2Texture() = default;

  bool Map(CVaapiRenderPicture *pic) override;
  void Unmap() override;

  GLuint GetTextureY() override;
  GLuint GetTextureVU() override;
  CSizeInt GetTextureSize() override;

  static void TestInterop(VADisplay vaDpy, EGLDisplay eglDisplay, bool &general, bool &deepColor);
  static bool TestInteropGeneral(VADisplay vaDpy, EGLDisplay eglDisplay);

private:
  static bool TestEsh(VADisplay vaDpy, EGLDisplay eglDisplay, std::uint32_t rtFormat, std::int32_t pixelFormat);

  struct MappedTexture
  {
    std::unique_ptr<CEGLImage> eglImage;
    GLuint glTexture{};
  };

  CVaapiRenderPicture* m_vaapiPic{};
  bool m_hasPlaneModifiers{false};
  std::array<KODI::UTILS::POSIX::CFileHandle, 4> m_drmFDs;
  MappedTexture m_y, m_vu;
  CSizeInt m_textureSize;
};

}

