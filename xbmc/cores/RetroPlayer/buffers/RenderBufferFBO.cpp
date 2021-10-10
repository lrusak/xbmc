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
#include "cores/RetroPlayer/buffers/RenderBufferPoolFBO.h"
#include "cores/RetroPlayer/rendering/RenderContext.h"
#include "cores/RetroPlayer/rendering/RenderVideoSettings.h"
#include "utils/log.h"
#include "windowing/WinSystem.h"
#include "windowing/linux/WinSystemEGL.h"

using namespace KODI;
using namespace RETRO;

CRenderBufferFBO::CRenderBufferFBO(CRenderContext& context) : m_context(context)
{
}

CRenderBufferFBO::~CRenderBufferFBO()
{
  DeleteTexture();
}

bool CRenderBufferFBO::Allocate(AVPixelFormat format, unsigned int width, unsigned int height)
{
  // Initialize IRenderBuffer
  m_format = format;
  m_width = width;
  m_height = height;

  if (!CreateTexture())
    return false;

  // if (!CreateFramebuffer())
  //   return false;

  // if (!CreateRenderbuffer())
  //   return false;

  // CLog::Log(LOGDEBUG, "RetroPlayer[RENDER]: allocate FBO buffer: {} fbo_id: {}", fmt::ptr(this), m_texture.fbo_id);

  // return CheckFrameBufferStatus();

  CLog::Log(LOGDEBUG, "RetroPlayer[RENDER]: allocate FBO buffer: {} tex_id: {}", fmt::ptr(this),
            m_texture.tex_id);


  return true;
}

void CRenderBufferFBO::Update()
{
  CLog::Log(LOGDEBUG, "RetroPlayer[RENDER]: update FBO buffer: {} tex_id: {}", fmt::ptr(this),
            m_texture.tex_id);

  auto pool = std::dynamic_pointer_cast<CRenderBufferPoolFBO>(m_pool);
  if (!pool)
    throw std::runtime_error("no pool!");

  if (!pool->IsConfigured2())
  {
    if (!pool->Configure(m_width, m_height))
      throw std::runtime_error("error configuring pool");
  }

  pool->BindFramebuffer();

  // attach the texture to FBO color attachment point
  glFramebufferTexture2D(GL_FRAMEBUFFER, // 1. fbo target: GL_FRAMEBUFFER
                         GL_COLOR_ATTACHMENT0, // 2. attachment point
                         GL_TEXTURE_2D, // 3. tex target: GL_TEXTURE_2D
                         m_texture.tex_id, // 4. tex ID
                         0); // 5. mipmap level: 0(base){

  pool->UnbindFramebuffer();
}

uintptr_t CRenderBufferFBO::GetCurrentFramebuffer()
{
  auto pool = std::dynamic_pointer_cast<CRenderBufferPoolFBO>(m_pool);
  if (!pool)
    throw std::runtime_error("no pool!");

  CLog::Log(LOGDEBUG, "RetroPlayer[RENDER]: GetCurrentFramebuffer: {} id: {}", fmt::ptr(this),
            pool->GetCurrentFramebuffer());

  return pool->GetCurrentFramebuffer();
}


void CRenderBufferFBO::DeleteTexture()
{
  glDeleteTextures(1, &m_texture.tex_id);
  m_texture.tex_id = 0;

  // glDeleteFramebuffers(1, &m_texture.fbo_id);
  // m_texture.fbo_id = 0;

  // glDeleteRenderbuffers(1, &m_texture.rbo_id);
  // m_texture.rbo_id = 0;
}

// bool CRenderBufferFBO::CreateFramebuffer()
// {
//   glGenFramebuffers(1, &m_texture.fbo_id);
//   glBindFramebuffer(GL_FRAMEBUFFER, m_texture.fbo_id);

//   // attach the texture to FBO color attachment point
//   glFramebufferTexture2D(GL_FRAMEBUFFER, // 1. fbo target: GL_FRAMEBUFFER
//                          GL_COLOR_ATTACHMENT0, // 2. attachment point
//                          GL_TEXTURE_2D, // 3. tex target: GL_TEXTURE_2D
//                          m_texture.tex_id, // 4. tex ID
//                          0); // 5. mipmap level: 0(base){

//   return true;
// }

// bool CRenderBufferFBO::CreateRenderbuffer()
// {
//   glGenRenderbuffers(1, &m_texture.rbo_id);
//   glBindRenderbuffer(GL_RENDERBUFFER, m_texture.rbo_id);
//   glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, m_width, m_height);
//   glBindFramebuffer(GL_FRAMEBUFFER, m_texture.fbo_id);

//   glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_texture.rbo_id);

//   return true;
// }

// bool CRenderBufferFBO::CheckFrameBufferStatus()
// {
//   GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
//   if(status != GL_FRAMEBUFFER_COMPLETE)
//   {
//     CLog::Log(LOGERROR, "RetroPlayer[RENDER]: Unable to create FBO - status: {}", status);
//     return false;
//   }

//   glBindRenderbuffer(GL_RENDERBUFFER, 0);
//   glBindFramebuffer(GL_FRAMEBUFFER, 0);

//   CLog::Log(LOGDEBUG, "CheckFrameBufferStatus(): {} fbo_id: {}", fmt::ptr(this), m_texture.fbo_id);

//   return true;
// }

bool CRenderBufferFBO::CreateTexture()
{
  glBindTexture(GL_TEXTURE_2D, 0);
  glGenTextures(1, &m_texture.tex_id);

  glBindTexture(GL_TEXTURE_2D, m_texture.tex_id);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glGenerateMipmap(GL_TEXTURE_2D);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

  return true;
}

bool CRenderBufferFBO::UploadTexture()
{
  // CLog::Log(LOGDEBUG, "UploadTexture(): {} fbo_id: {}", fmt::ptr(this), m_texture.fbo_id);

  return true;
}
