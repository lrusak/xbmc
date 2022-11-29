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
  glDeleteTextures(1, &m_tex_id);
  m_tex_id = 0;

  glDeleteRenderbuffers(1, &m_rbo_id);
  m_rbo_id = 0;
}

bool CRenderBufferFBO::Allocate(AVPixelFormat format, unsigned int width, unsigned int height)
{
  // Initialize IRenderBuffer
  m_format = format;
  m_width = width;
  m_height = height;

  if (!CreateTexture())
    return false;

  if (!CreateRenderbuffer())
    return false;

  return true;
}

void CRenderBufferFBO::Update()
{
  glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_id);

  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_tex_id, 0);

  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_rbo_id);

  if (!CheckFrameBufferStatus())
    throw std::runtime_error("whoops!");
}

bool CRenderBufferFBO::CreateTexture()
{
  glBindTexture(GL_TEXTURE_2D, 0);
  glGenTextures(1, &m_tex_id);

  glBindTexture(GL_TEXTURE_2D, m_tex_id);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glGenerateMipmap(GL_TEXTURE_2D);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

  return true;
}

bool CRenderBufferFBO::CreateRenderbuffer()
{
  glGenRenderbuffers(1, &m_rbo_id);
  glBindRenderbuffer(GL_RENDERBUFFER, m_rbo_id);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, m_width, m_height);

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

  return true;
}

uintptr_t CRenderBufferFBO::GetCurrentFramebuffer()
{
  return m_fbo_id;
}
