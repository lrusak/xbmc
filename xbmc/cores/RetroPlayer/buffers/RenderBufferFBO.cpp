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

CRenderBufferFBO::CRenderBufferFBO(CRenderContext& context, uint32_t fbo_id)
  : m_context(context), m_fbo_id(fbo_id)
{
}

CRenderBufferFBO::~CRenderBufferFBO()
{
  glDeleteRenderbuffers(1, &m_rbo_id);
  m_rbo_id = 0;

  m_image->DestroyImage();

  m_buffer->DestroyBufferObject();
}

bool CRenderBufferFBO::Allocate(AVPixelFormat format, unsigned int width, unsigned int height)
{
  // Initialize IRenderBuffer
  m_format = format;
  m_width = width;
  m_height = height;

  m_buffer = CBufferObjectFactory::CreateBufferObject(false);
  if (!m_buffer->CreateBufferObject(DRM_FORMAT_ARGB8888, width, height))
  {
    CLog::Log(LOGERROR, "RetroPlayer[RENDER]: failed to create buffer");
    return false;
  }

  auto winSystem = CServiceBroker::GetWinSystem();
  auto eglWinSystem = dynamic_cast<WINDOWING::LINUX::CWinSystemEGL*>(winSystem);

  m_image = std::make_unique<CEGLImage>(eglWinSystem->GetEGLDisplay());

  std::array<CEGLImage::EglPlane, CEGLImage::MAX_NUM_PLANES> planes;
  planes[0].fd = m_buffer->GetFd();
  planes[0].pitch = m_buffer->GetStride();
  planes[0].offset = 0;
  planes[0].modifier = m_buffer->GetModifier();

  CEGLImage::EglAttrs attrs;
  attrs.width = width;
  attrs.height = height;
  attrs.format = DRM_FORMAT_ARGB8888;
  attrs.planes = planes;

  if (!m_image->CreateImage(attrs))
  {
    CLog::Log(LOGERROR, "RetroPlayer[RENDER]: failed to create image");
    return false;
  }

  if (!CreateRenderbuffer())
    return false;

  return true;
}

bool CRenderBufferFBO::CreateRenderbuffer()
{
  glGenRenderbuffers(1, &m_rbo_id);
  glBindRenderbuffer(GL_RENDERBUFFER, m_rbo_id);
  m_image->AttachRenderBuffer(GL_RENDERBUFFER);

  return true;
}

bool CRenderBufferFBO::CheckFrameBufferStatus()
{
  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE)
  {
    CLog::Log(LOGERROR, "RetroPlayer[RENDER]: fbo error - status: {}", status);
    return false;
  }

  return m_fbo_id;
}

CEGLImage* CRenderBufferFBO::GetImage() const
{
  return m_image.get();
}
