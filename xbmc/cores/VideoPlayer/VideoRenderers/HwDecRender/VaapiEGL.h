/*
 *  Copyright (C) 2007-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <array>

#if defined(HAS_GL)
#include <GL/gl.h>
#elif defined(HAS_GLES)
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#endif

#include "utils/EGLImage.h"

#include <va/va.h>

#include "utils/Geometry.h"
#include "platform/posix/utils/FileHandle.h"

#include <memory>

namespace VAAPI
{

class CVaapiRenderPicture;

class CVaapiTexture
{
public:
  CVaapiTexture() = default;
  virtual ~CVaapiTexture() = default;

  virtual void Init(EGLDisplay display) = 0;
  virtual bool Map(CVaapiRenderPicture *pic) = 0;
  virtual void Unmap() = 0;

  virtual int GetBits() = 0;
  virtual GLuint GetTextureY() = 0;
  virtual GLuint GetTextureVU() = 0;
  virtual CSizeInt GetTextureSize() = 0;

protected:
  int GetColorSpace(int colorSpace);
  int GetColorRange(int colorRange);
};

class CVaapi1Texture : public CVaapiTexture
{
public:
  CVaapi1Texture();

  bool Map(CVaapiRenderPicture *pic) override;
  void Unmap() override;
  void Init(EGLDisplay eglDisplay) override;

  int GetBits() override;
  GLuint GetTextureY() override;
  GLuint GetTextureVU() override;
  CSizeInt GetTextureSize() override;

  static void TestInterop(VADisplay vaDpy, EGLDisplay eglDisplay, bool &general, bool &deepColor);

  GLuint m_textureY = 0;
  GLuint m_textureVU = 0;
  int m_texWidth = 0;
  int m_texHeight = 0;
  int m_bits = 0;

protected:
  static bool TestInteropDeepColor(VADisplay vaDpy, EGLDisplay eglDisplay);

  GLenum m_textureTarget{GL_TEXTURE_2D};
  CVaapiRenderPicture *m_vaapiPic = nullptr;
  struct GLSurface
  {
    VAImage vaImage{VA_INVALID_ID};
    VABufferInfo vBufInfo;
    std::unique_ptr<CEGLImage> eglImageY;
    std::unique_ptr<CEGLImage> eglImageVU;
  } m_glSurface;
};

class CVaapi2Texture : public CVaapiTexture
{
public:
  bool Map(CVaapiRenderPicture *pic) override;
  void Unmap() override;
  void Init(EGLDisplay eglDisplay) override;

  int GetBits() override;
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
    GLuint glTexture{0};
  };

  GLenum m_textureTarget{GL_TEXTURE_2D};
  CVaapiRenderPicture* m_vaapiPic{};
  std::array<KODI::UTILS::POSIX::CFileHandle, 4> m_drmFDs;
  int m_bits{0};
  MappedTexture m_y, m_vu;
  CSizeInt m_textureSize;
};

}

