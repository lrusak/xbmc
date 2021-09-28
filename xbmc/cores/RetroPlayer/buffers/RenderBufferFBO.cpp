/*
 *      Copyright (C) 2017 Team Kodi
 *      http://kodi.tv
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this Program; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "RenderBufferFBO.h"

#include "ServiceBroker.h"
#include "cores/RetroPlayer/rendering/RenderContext.h"
#include "cores/RetroPlayer/rendering/RenderVideoSettings.h"
#include "utils/BufferObjectFactory.h"
#include "utils/EGLImage.h"
#include "utils/log.h"
#include "windowing/WinSystem.h"
#include "windowing/linux/WinSystemEGL.h"

// #include "Util.h"
// #include "utils/URIUtils.h"
// #include "pictures/Picture.h"
// #include "URL.h"

using namespace KODI;
using namespace RETRO;

CRenderBufferFBO::CRenderBufferFBO(CRenderContext &context, EGLDisplay eglDisplay) :
  m_context(context)
{
  m_eglImage = std::make_unique<CEGLImage>(eglDisplay);
  if (!m_eglImage)
    throw std::runtime_error("something is terribly wrong");

  m_buffer = CBufferObject::GetBufferObject(true);
  if (!m_buffer)
    throw std::runtime_error("buffer object creation failed");
}

bool CRenderBufferFBO::Allocate(AVPixelFormat format, unsigned int width, unsigned int height)
{
  // Initialize IRenderBuffer
  m_format = format;
  m_width = width;
  m_height = height;

  if (!CreateDMABuf())
    return false;

  // if (!CreateTexture())
  //   return false;

  if (!CreateRenderbuffer())
    return false;

  if (!CreateFramebuffer())
    return false;

  CLog::Log(LOGDEBUG, "RetroPlayer[RENDER]: allocate FBO buffer: {} fbo_id: {}", fmt::ptr(this), m_texture.fbo_id);

  return CheckFrameBufferStatus();
}

void CRenderBufferFBO::DeleteTexture()
{
  glDeleteTextures(1, &m_texture.tex_id);
  m_texture.tex_id = 0;

  glDeleteFramebuffers(1, &m_texture.fbo_id);
  m_texture.fbo_id = 0;

  glDeleteRenderbuffers(1, &m_texture.rbo_id);
  m_texture.rbo_id = 0;
}

// bool CRenderBufferFBO::UploadTexture()
// {
//   if (!glIsTexture(m_texture.tex_id))
//     glGenTextures(1, &m_texture.tex_id);

//   m_eglImage->UploadImage(m_textureTarget);

//   return true;
// }

// bool CRenderBufferFBO::CreateTexture()
// {
//   return true;
// }

bool CRenderBufferFBO::CreateDMABuf()
{
  if (!m_buffer->CreateBufferObject(m_width * m_height * 4))
    return false;

  std::array<CEGLImage::EglPlane, CEGLImage::MAX_NUM_PLANES> planes;
  planes[0].fd = m_buffer->GetFd();
  planes[0].offset = 0;
  planes[0].pitch = m_width * 4;
  planes[0].modifier = DRM_FORMAT_MOD_LINEAR;

  CEGLImage::EglAttrs attributes;

  attributes.format = DRM_FORMAT_XRGB8888;
  attributes.height = m_height;
  attributes.width = m_width;
  attributes.planes = planes;

  if (!m_eglImage->CreateImage(attributes))
    return false;

  return true;
}

bool CRenderBufferFBO::CreateFramebuffer()
{
  glGenFramebuffers(1, &m_texture.fbo_id);
  glBindFramebuffer(GL_FRAMEBUFFER, m_texture.fbo_id);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                            m_texture.rbo_id);

  return true;
}

bool CRenderBufferFBO::CreateRenderbuffer()
{
  glGenRenderbuffers(1, &m_texture.rbo_id);
  glBindRenderbuffer(GL_RENDERBUFFER, m_texture.rbo_id);
  m_eglImage->AttachRenderBufferStorage(GL_RENDERBUFFER);

  return true;
}

bool CRenderBufferFBO::CheckFrameBufferStatus()
{
  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if(status != GL_FRAMEBUFFER_COMPLETE)
  {
    CLog::Log(LOGERROR, "RetroPlayer[RENDER]: Unable to create FBO - status: %d", status);
    return false;
  }

  glBindRenderbuffer(GL_RENDERBUFFER, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  return true;
}

void CRenderBufferFBO::CreateTexture()
{
  glGenTextures(1, &m_texture.tex_id);

  glBindTexture(m_textureTarget, m_texture.tex_id);

  glTexParameteri(m_textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(m_textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(m_textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(m_textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glBindTexture(m_textureTarget, 0);
}

bool CRenderBufferFBO::UploadTexture()
{
  if (m_buffer->GetFd() < 0)
    return false;

  // std::string file =
  //     CUtil::GetNextFilename(URIUtils::AddFileToFolder("/home/lukas/", "dma-{:05}.png"), 65535);

  // auto memory = m_buffer->GetMemory();
  // auto stride = m_buffer->GetStride();

  // // test dma contents
  // if (!CPicture::CreateThumbnailFromSurface(memory, m_width, m_height, stride, file))
  //   CLog::Log(LOGERROR, "Unable to write dma {}", CURL::GetRedacted(file));

  // m_buffer->ReleaseMemory();

  if (!glIsTexture(m_texture.tex_id))
    CreateTexture();

  glBindTexture(m_textureTarget, m_texture.tex_id);

  CLog::Log(LOGDEBUG, "RetroPlayer[RENDER]: upload FBO buffer: {} fbo_id: {}", fmt::ptr(this), m_texture.fbo_id);

  m_eglImage->UploadImage(m_textureTarget);

  // std::array<CEGLImage::EglPlane, CEGLImage::MAX_NUM_PLANES> planes;

  // planes[0].fd = m_buffer->GetFd();
  // planes[0].offset = 0;
  // planes[0].pitch = m_width * 4;
  // planes[0].modifier = m_buffer->GetModifier();

  // CEGLImage::EglAttrs attribs;

  // attribs.width = m_width;
  // attribs.height = m_height;
  // attribs.format = DRM_FORMAT_ARGB8888;
  // attribs.planes = planes;

  // if (!m_egl)
  // {
  //   auto winSystemEGL =
  //       dynamic_cast<KODI::WINDOWING::LINUX::CWinSystemEGL*>(CServiceBroker::GetWinSystem());

  //   if (winSystemEGL == nullptr)
  //     throw std::runtime_error("dynamic_cast failed to cast to CWinSystemEGL. This is likely due to "
  //                             "a build misconfiguration as DMA can only be used with EGL and "
  //                             "specifically platforms that implement CWinSystemEGL");

  //   m_egl = std::make_unique<CEGLImage>(winSystemEGL->GetEGLDisplay());
  // }

  // if (m_egl->CreateImage(attribs))
  //   m_egl->UploadImage(m_textureTarget);

  // m_egl->DestroyImage();

  glBindTexture(m_textureTarget, 0);

  return true;
}
