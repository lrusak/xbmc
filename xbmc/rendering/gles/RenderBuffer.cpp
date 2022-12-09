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
#include "utils/BufferObject.h"
#include "utils/BufferObjectFactory.h"
#include "utils/EGLImage.h"
#include "utils/GLUtils.h"
#include "utils/log.h"
#include "windowing/WinSystem.h"
#include "windowing/linux/WinSystemEGL.h"

#include <algorithm>
#include <stdexcept>

namespace
{

// clang-format off
const GLfloat vVertices[] = {
  // front
  -1.0f, -1.0f, +1.0f,
  +1.0f, -1.0f, +1.0f,
  -1.0f, +1.0f, +1.0f,
  +1.0f, +1.0f, +1.0f,
  // back
  +1.0f, -1.0f, -1.0f,
  -1.0f, -1.0f, -1.0f,
  +1.0f, +1.0f, -1.0f,
  -1.0f, +1.0f, -1.0f,
  // right
  +1.0f, -1.0f, +1.0f,
  +1.0f, -1.0f, -1.0f,
  +1.0f, +1.0f, +1.0f,
  +1.0f, +1.0f, -1.0f,
  // left
  -1.0f, -1.0f, -1.0f,
  -1.0f, -1.0f, +1.0f,
  -1.0f, +1.0f, -1.0f,
  -1.0f, +1.0f, +1.0f,
  // top
  -1.0f, +1.0f, +1.0f,
  +1.0f, +1.0f, +1.0f,
  -1.0f, +1.0f, -1.0f,
  +1.0f, +1.0f, -1.0f,
  // bottom
  -1.0f, -1.0f, -1.0f,
  +1.0f, -1.0f, -1.0f,
  -1.0f, -1.0f, +1.0f,
  +1.0f, -1.0f, +1.0f,
};

const GLfloat vTexCoords[] = {
  //front
  1.0f, 1.0f,
  0.0f, 1.0f,
  1.0f, 0.0f,
  0.0f, 0.0f,
  //back
  1.0f, 1.0f,
  0.0f, 1.0f,
  1.0f, 0.0f,
  0.0f, 0.0f,
  //right
  1.0f, 1.0f,
  0.0f, 1.0f,
  1.0f, 0.0f,
  0.0f, 0.0f,
  //left
  1.0f, 1.0f,
  0.0f, 1.0f,
  1.0f, 0.0f,
  0.0f, 0.0f,
  //top
  1.0f, 1.0f,
  0.0f, 1.0f,
  1.0f, 0.0f,
  0.0f, 0.0f,
  //bottom
  1.0f, 0.0f,
  0.0f, 0.0f,
  1.0f, 1.0f,
  0.0f, 1.0f,
};

const GLfloat vNormals[] = {
  // front
  +0.0f, +0.0f, +1.0f, // forward
  +0.0f, +0.0f, +1.0f, // forward
  +0.0f, +0.0f, +1.0f, // forward
  +0.0f, +0.0f, +1.0f, // forward
  // back
  +0.0f, +0.0f, -1.0f, // backward
  +0.0f, +0.0f, -1.0f, // backward
  +0.0f, +0.0f, -1.0f, // backward
  +0.0f, +0.0f, -1.0f, // backward
  // right
  +1.0f, +0.0f, +0.0f, // right
  +1.0f, +0.0f, +0.0f, // right
  +1.0f, +0.0f, +0.0f, // right
  +1.0f, +0.0f, +0.0f, // right
  // left
  -1.0f, +0.0f, +0.0f, // left
  -1.0f, +0.0f, +0.0f, // left
  -1.0f, +0.0f, +0.0f, // left
  -1.0f, +0.0f, +0.0f, // left
  // top
  +0.0f, +1.0f, +0.0f, // up
  +0.0f, +1.0f, +0.0f, // up
  +0.0f, +1.0f, +0.0f, // up
  +0.0f, +1.0f, +0.0f, // up
  // bottom
  +0.0f, -1.0f, +0.0f, // down
  +0.0f, -1.0f, +0.0f, // down
  +0.0f, -1.0f, +0.0f, // down
  +0.0f, -1.0f, +0.0f  // down
};
// clang-format on

} // namespace

CRenderBuffer::CRenderBuffer()
{
  CLog::Log(LOGDEBUG, "CRenderBuffer::{} - addr: {}", __FUNCTION__, fmt::ptr(this));
}

CRenderBuffer::~CRenderBuffer()
{
  CLog::Log(LOGDEBUG, "CRenderBuffer::{} - addr: {}", __FUNCTION__, fmt::ptr(this));

  if (m_fboid != 0)
    glDeleteFramebuffers(1, &m_fboid);

  if (m_rboid != 0)
    glDeleteTextures(1, &m_rboid);

  m_image->DestroyImage();
  m_buffer->DestroyBufferObject();
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

  m_buffer = CBufferObjectFactory::CreateBufferObject(false);
  if (!m_buffer->CreateBufferObject(DRM_FORMAT_ARGB8888, width, height))
  {
    return false;
  }

  std::array<CEGLImage::EglPlane, CEGLImage::MAX_NUM_PLANES> planes;

  planes[0].fd = m_buffer->GetFd();
  planes[0].offset = 0;
  planes[0].pitch = m_buffer->GetStride();
  planes[0].modifier = m_buffer->GetModifier();

  CEGLImage::EglAttrs attribs;

  attribs.width = m_width;
  attribs.height = m_height;
  attribs.format = DRM_FORMAT_ARGB8888;
  attribs.planes = planes;

  auto winSystemEGL =
      dynamic_cast<KODI::WINDOWING::LINUX::CWinSystemEGL*>(CServiceBroker::GetWinSystem());
  if (!winSystemEGL)
  {
    return false;
  }

  m_image = std::make_unique<CEGLImage>(winSystemEGL->GetEGLDisplay());

  if (!m_image->CreateImage(attribs))
    return false;

  glGenRenderbuffers(1, &m_rboid);
  glBindRenderbuffer(GL_RENDERBUFFER, m_rboid);
  m_image->AttachRenderBuffer(GL_RENDERBUFFER);

  glGenFramebuffers(1, &m_fboid);
  glBindFramebuffer(GL_FRAMEBUFFER, m_fboid);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_rboid);

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
  m_image->UploadImage(GL_TEXTURE_2D);
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

  glMatrixProject->Rotatef(-180.0f, 0, 0, 1.0f);
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

  glBindTexture(GL_TEXTURE_2D, 0);

  renderSystemGLES->DisableGUIShader();

  glMatrixModview.PopLoad();
  glMatrixProject.PopLoad();

  renderSystemGLES->SetViewPort(viewport);

  return true;
}

bool CRenderBuffer::RenderCube()
{
  // CLog::Log(LOGDEBUG, "CRenderBuffer::{} - addr: {}", __FUNCTION__, fmt::ptr(this));

  auto renderSystem = CServiceBroker::GetRenderSystem();
  if (!renderSystem)
    return false;

  auto renderSystemGLES = static_cast<CRenderSystemGLES*>(renderSystem);
  if (!renderSystemGLES)
    return false;

  glEnable(GL_CULL_FACE);

  glMatrixModview.Push();
  glMatrixModview->LoadIdentity();
  glMatrixModview->Translatef(0.0f, 0.0f, -8.0f);
  glMatrixModview->Rotatef(45.0f + (0.25f * m_i), 1.0f, 0.0f, 0.0f);
  glMatrixModview->Rotatef(45.0f - (0.5f * m_i), 0.0f, 1.0f, 0.0f);
  glMatrixModview->Rotatef(10.0f + (0.15f * m_i), 0.0f, 0.0f, 1.0f);
  glMatrixModview.Load();

  float aspect = static_cast<float>(m_height) / m_width;

  glMatrixProject.Push();
  glMatrixProject->LoadIdentity();
  glMatrixProject->Frustum(-2.8f, +2.8f, -2.8f * aspect, +2.8f * aspect, 6.0f, 10.0f);
  // glMatrixProject->Ortho2D(0, m_width, 0, m_height);
  glMatrixProject.Load();

  glMatrixModviewProjection.Push();
  glMatrixModviewProjection->LoadIdentity();
  std::memcpy(&glMatrixModviewProjection.Get(), &glMatrixProject.Get(), sizeof(CMatrixGL));
  glMatrixModviewProjection->MultMatrixf(glMatrixModview.Get());
  glMatrixModviewProjection.Load();

  VerifyGLState();

  CMatrixGL& modview = glMatrixModview.Get();

  float normal[9];
  normal[0] = modview[0];
  normal[1] = modview[1];
  normal[2] = modview[2];
  normal[3] = modview[4];
  normal[4] = modview[5];
  normal[5] = modview[6];
  normal[6] = modview[8];
  normal[7] = modview[9];
  normal[8] = modview[10];

  CRect viewport;
  renderSystemGLES->GetViewPort(viewport);
  glViewport(0, 0, m_width, m_height);
  glScissor(0, 0, m_width, m_height);

  renderSystemGLES->EnableGUIShader(ShaderMethodGLES::SM_CUBE);

  GLint modelMatrix = renderSystemGLES->GUIShaderGetModel();
  GLint projMatrix = renderSystemGLES->GUIShaderGetProjectionMatrix();
  GLint normalMatrix = renderSystemGLES->GUIShaderGetNormalMatrix();

  GLint pos = renderSystemGLES->GUIShaderGetPos();
  GLint normalPos = renderSystemGLES->GUIShaderGetNormal();
  GLint coord = renderSystemGLES->GUIShaderGetCoord0();

  GLuint positionsoffset = 0;
  GLuint texcoordsoffset = sizeof(vVertices);
  GLuint normalsoffset = sizeof(vVertices) + sizeof(vTexCoords);

  GLuint vbo;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vVertices) + sizeof(vTexCoords) + sizeof(vNormals), 0,
               GL_STATIC_DRAW);
  glBufferSubData(GL_ARRAY_BUFFER, positionsoffset, sizeof(vVertices), &vVertices[0]);
  glBufferSubData(GL_ARRAY_BUFFER, texcoordsoffset, sizeof(vTexCoords), &vTexCoords[0]);
  glBufferSubData(GL_ARRAY_BUFFER, normalsoffset, sizeof(vNormals), &vNormals[0]);
  glVertexAttribPointer(pos, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid*)(intptr_t)positionsoffset);
  glEnableVertexAttribArray(pos);
  glVertexAttribPointer(normalPos, 3, GL_FLOAT, GL_FALSE, 0,
                        (const GLvoid*)(intptr_t)normalsoffset);
  glEnableVertexAttribArray(normalPos);
  glVertexAttribPointer(coord, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid*)(intptr_t)texcoordsoffset);
  glEnableVertexAttribArray(coord);

  /* clear the color buffer */
  glClearColor(0.0, 0.0, 0.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);

  const GLfloat* modviewMatrix = glMatrixModview.Get();
  const GLfloat* modviewprojMatrix = glMatrixModviewProjection.Get();

  glUniformMatrix4fv(modelMatrix, 1, GL_FALSE, modviewMatrix);
  glUniformMatrix4fv(projMatrix, 1, GL_FALSE, modviewprojMatrix);
  glUniformMatrix3fv(normalMatrix, 1, GL_FALSE, normal);

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);
  glDrawArrays(GL_TRIANGLE_STRIP, 8, 4);
  glDrawArrays(GL_TRIANGLE_STRIP, 12, 4);
  glDrawArrays(GL_TRIANGLE_STRIP, 16, 4);
  glDrawArrays(GL_TRIANGLE_STRIP, 20, 4);

  glDisableVertexAttribArray(pos);
  glDisableVertexAttribArray(normalPos);
  glDisableVertexAttribArray(coord);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glDeleteBuffers(1, &vbo);

  glMatrixModview.PopLoad();
  glMatrixProject.PopLoad();
  glMatrixModviewProjection.PopLoad();

  renderSystemGLES->SetViewPort(viewport);

  glDisable(GL_CULL_FACE);

  renderSystemGLES->DisableGUIShader();

  m_i++;

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
