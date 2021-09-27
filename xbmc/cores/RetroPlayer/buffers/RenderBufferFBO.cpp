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

using namespace KODI;
using namespace RETRO;

CRenderBufferFBO::CRenderBufferFBO(CRenderContext &context) :
  m_context(context)
{
  auto winSystem =
      dynamic_cast<KODI::WINDOWING::LINUX::CWinSystemEGL*>(CServiceBroker::GetWinSystem());
  if (!winSystem)
    throw std::runtime_error("this shouldn't happen!");

  m_eglImage = std::make_unique<CEGLImage>(winSystem);
  if (!m_eglImage)
    throw std::runtime_error("something is terribly wrong");

  m_buffer = CBufferObject::GetBufferObject(false);
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

  if (!CreateTexture())
    return false;

  if (!CreateRenderbuffer())
    return false;

  if (!CreateFramebuffer())
    return false;

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

bool CRenderBufferFBO::UploadTexture()
{
  if (!glIsTexture(m_texture.tex_id))
    glGenTextures(1, &m_texture.tex_id);

  m_eglImage->UploadImage(m_textureTarget);

  return true;
}

bool CRenderBufferFBO::CreateTexture()
{
  return true;
}

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

  attributes.format = DRM_FORMAT_RGBA8888;
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
