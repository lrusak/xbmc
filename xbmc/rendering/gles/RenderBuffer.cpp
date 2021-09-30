/*
 *  Copyright (C) 2005-2021 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "RenderBuffer.h"

#include "ServiceBroker.h"
#include "rendering/MatrixGL.h"
#include "rendering/RenderSystem.h"
#include "rendering/gles/RenderSystemGLES.h"
#include "utils/log.h"

#include <algorithm>
#include <stdexcept>

CRenderBuffer::CRenderBuffer()
{
  CLog::Log(LOGDEBUG, "CRenderBuffer::{} - addr: {}", __FUNCTION__, fmt::ptr(this));
}

CRenderBuffer::~CRenderBuffer()
{
  CLog::Log(LOGDEBUG, "CRenderBuffer::{} - addr: {}", __FUNCTION__, fmt::ptr(this));

  if (m_fboid != 0)
    glDeleteFramebuffers(1, &m_fboid);

  if (m_texid != 0)
    glDeleteTextures(1, &m_texid);
}

bool CRenderBuffer::Allocate(uint32_t width, uint32_t height)
{
  CLog::Log(LOGDEBUG, "CRenderBuffer::{} - addr: {} width: {} height: {}", __FUNCTION__,
            fmt::ptr(this), width, height);

  m_width = width;
  m_height = height;

  glGenTextures(1, &m_texid);
  glBindTexture(GL_TEXTURE_2D, m_texid);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glBindTexture(GL_TEXTURE_2D, 0);

  glGenFramebuffers(1, &m_fboid);
  glBindFramebuffer(GL_FRAMEBUFFER, m_fboid);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texid, 0);

  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  if (status != GL_FRAMEBUFFER_COMPLETE)
    return false;

  return true;
}

void CRenderBuffer::Release()
{
  CLog::Log(LOGDEBUG, "CRenderBuffer::{} - addr: {}", __FUNCTION__, fmt::ptr(this));

  m_pool->Return(shared_from_this());
}

void CRenderBuffer::BindFrameBuffer()
{
  // CLog::Log(LOGDEBUG, "CRenderBuffer::{} - addr: {} id: {}", __FUNCTION__, fmt::ptr(this), m_fboid);

  glBindFramebuffer(GL_FRAMEBUFFER, m_fboid);
}

void CRenderBuffer::UnbindFrameBuffer()
{
  // CLog::Log(LOGDEBUG, "CRenderBuffer::{} - addr: {} id: {}", __FUNCTION__, fmt::ptr(this), m_fboid);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void CRenderBuffer::BindTexture()
{
  // CLog::Log(LOGDEBUG, "CRenderBuffer::{} - addr: {} id: {}", __FUNCTION__, fmt::ptr(this), m_texid);

  glBindTexture(GL_TEXTURE_2D, m_texid);
}

void CRenderBuffer::UnbindTexture()
{
  // CLog::Log(LOGDEBUG, "CRenderBuffer::{} - addr: {} id: {}", __FUNCTION__, fmt::ptr(this), m_texid);

  glBindTexture(GL_TEXTURE_2D, 0);
}

bool CRenderBuffer::IsCompatible(uint32_t width, uint32_t height)
{
  CLog::Log(LOGDEBUG, "CRenderBuffer::{} - addr: {}", __FUNCTION__, fmt::ptr(this));

  return m_width == width && m_height == height;
}

bool CRenderBuffer::Render()
{
  // CLog::Log(LOGDEBUG, "CRenderBuffer::{} - addr: {}", __FUNCTION__, fmt::ptr(this));

  auto renderSystem = CServiceBroker::GetRenderSystem();
  if (!renderSystem)
    return false;

  auto renderSystemGLES = static_cast<CRenderSystemGLES*>(renderSystem);
  if (!renderSystemGLES)
    return false;

  glMatrixModview.Push();
  glMatrixModview->LoadIdentity();
  glMatrixModview.Load();

  glMatrixProject.Push();
  glMatrixProject->LoadIdentity();
  glMatrixProject->Ortho2D(0, m_width, 0, m_height);
  glMatrixProject.Load();

  CRect viewport;
  renderSystemGLES->GetViewPort(viewport);
  glViewport(0, 0, m_width, m_height);
  glScissor(0, 0, m_width, m_height);

  renderSystemGLES->EnableGUIShader(ShaderMethodGLES::SM_TEXTURE);

  GLubyte idx[4] = {0, 1, 3, 2}; // determines order of triangle strip
  GLfloat vert[4][3];
  GLfloat tex[4][2];

  GLint vertLoc = renderSystemGLES->GUIShaderGetPos();
  GLint loc = renderSystemGLES->GUIShaderGetCoord0();
  GLint uniColLoc = renderSystemGLES->GUIShaderGetUniCol();

  glVertexAttribPointer(vertLoc, 3, GL_FLOAT, 0, 0, vert);
  glVertexAttribPointer(loc, 2, GL_FLOAT, 0, 0, tex);

  glEnableVertexAttribArray(vertLoc);
  glEnableVertexAttribArray(loc);

  CPoint rect[4];
  rect[0].x = 0.0f;
  rect[0].y = 0.0f;

  rect[1].x = static_cast<float>(m_width);
  rect[1].y = 0.0f;

  rect[2].x = static_cast<float>(m_width);
  rect[2].y = static_cast<float>(m_height);

  rect[3].x = 0.0f;
  rect[3].y = static_cast<float>(m_height);

  // Setup vertex position values
  for (int i = 0; i < 4; i++)
  {
    vert[i][0] = rect[i].x;
    vert[i][1] = rect[i].y;
    vert[i][2] = 0.0f; // set z to 0
  }

  // Setup texture coordinates
  tex[0][0] = tex[3][0] = 0.0f;
  tex[0][1] = tex[1][1] = 0.0f;
  tex[1][0] = tex[2][0] = 1.0f;
  tex[2][1] = tex[3][1] = 1.0f;

  glUniform4f(uniColLoc, 1.0f, 1.0f, 1.0f, 1.0f);
  glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_BYTE, idx);

  glDisableVertexAttribArray(vertLoc);
  glDisableVertexAttribArray(loc);

  renderSystemGLES->DisableGUIShader();

  glMatrixModview.PopLoad();
  glMatrixProject.PopLoad();

  renderSystemGLES->SetViewPort(viewport);

  return true;
}

CRenderBufferPool::CRenderBufferPool() = default;
CRenderBufferPool::~CRenderBufferPool() = default;

std::shared_ptr<CRenderBuffer> CRenderBufferPool::GetBuffer(uint32_t width, uint32_t height)
{
  CLog::Log(LOGDEBUG, "CRenderBufferPool::{} - start", __FUNCTION__);

  if (!m_free.empty())
    CLog::Log(LOGDEBUG, "CRenderBufferPool::{} - m_free:", __FUNCTION__);

  for (const auto& free : m_free)
    CLog::Log(LOGDEBUG, "CRenderBufferPool::{} - \taddr: {} use: {}", __FUNCTION__, fmt::ptr(free),
              free.use_count());

  if (!m_used.empty())
    CLog::Log(LOGDEBUG, "CRenderBufferPool::{} - m_used:", __FUNCTION__);

  for (const auto& used : m_used)
    CLog::Log(LOGDEBUG, "CRenderBufferPool::{} - \taddr: {} use: {}", __FUNCTION__, fmt::ptr(used),
              used.use_count());

  std::shared_ptr<CRenderBuffer> buffer = nullptr;

  if (!m_free.empty())
  {
    buffer = std::move(m_free.front());
    m_free.pop_front();

    if (!buffer->IsCompatible(width, height))
    {
      buffer.reset();
      buffer = nullptr;
    }
  }

  if (!buffer)
  {
    buffer = std::make_shared<CRenderBuffer>();
    if (!buffer->Allocate(width, height))
      throw std::runtime_error("whoops!");

    buffer->SetPool(shared_from_this());
  }

  m_used.emplace_back(buffer);

  CLog::Log(LOGDEBUG, "CRenderBufferPool::{} - end", __FUNCTION__);

  if (!m_free.empty())
    CLog::Log(LOGDEBUG, "CRenderBufferPool::{} - m_free:", __FUNCTION__);

  for (const auto& free : m_free)
    CLog::Log(LOGDEBUG, "CRenderBufferPool::{} - \taddr: {} use: {}", __FUNCTION__, fmt::ptr(free),
              free.use_count());

  if (!m_used.empty())
    CLog::Log(LOGDEBUG, "CRenderBufferPool::{} - m_used:", __FUNCTION__);

  for (const auto& used : m_used)
    CLog::Log(LOGDEBUG, "CRenderBufferPool::{} - \taddr: {} use: {}", __FUNCTION__, fmt::ptr(used),
              used.use_count());

  return buffer;
}

void CRenderBufferPool::Return(std::shared_ptr<CRenderBuffer> buffer)
{
  CLog::Log(LOGDEBUG, "CRenderBufferPool::{} - start:", __FUNCTION__);

  if (!m_free.empty())
    CLog::Log(LOGDEBUG, "CRenderBufferPool::{} - m_free:", __FUNCTION__);

  for (const auto& free : m_free)
    CLog::Log(LOGDEBUG, "CRenderBufferPool::{} - \taddr: {} use: {}", __FUNCTION__, fmt::ptr(free),
              free.use_count());

  if (!m_used.empty())
    CLog::Log(LOGDEBUG, "CRenderBufferPool::{} - m_used:", __FUNCTION__);

  for (const auto& used : m_used)
    CLog::Log(LOGDEBUG, "CRenderBufferPool::{} - \taddr: {} use: {}", __FUNCTION__, fmt::ptr(used),
              used.use_count());

  m_free.emplace_back(buffer);

  auto buf = std::find(m_used.begin(), m_used.end(), buffer);
  if (buf != m_used.end())
  {
    CLog::Log(LOGDEBUG, "CRenderBufferPool::{} - buffer removed addr: {}", __FUNCTION__,
              fmt::ptr(buffer));
    m_used.erase(buf);
  }

  CLog::Log(LOGDEBUG, "CRenderBufferPool::{} - end:", __FUNCTION__);

  if (!m_free.empty())
    CLog::Log(LOGDEBUG, "CRenderBufferPool::{} - m_free:", __FUNCTION__);

  for (const auto& free : m_free)
    CLog::Log(LOGDEBUG, "CRenderBufferPool::{} - \taddr: {} use: {}", __FUNCTION__, fmt::ptr(free),
              free.use_count());

  if (!m_used.empty())
    CLog::Log(LOGDEBUG, "CRenderBufferPool::{} - m_used:", __FUNCTION__);

  for (const auto& used : m_used)
    CLog::Log(LOGDEBUG, "CRenderBufferPool::{} - \taddr: {} use: {}", __FUNCTION__, fmt::ptr(used),
              used.use_count());
}
