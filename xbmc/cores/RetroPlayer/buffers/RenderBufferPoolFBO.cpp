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

#include "RenderBufferPoolFBO.h"

#include "RenderBufferFBO.h"
#include "ServiceBroker.h"
#include "cores/RetroPlayer/rendering/RenderContext.h"
#include "utils/log.h"
#include "windowing/WinSystem.h"
#include "windowing/linux/WinSystemEGL.h"

using namespace KODI;
using namespace RETRO;

CRenderBufferPoolFBO::CRenderBufferPoolFBO(CRenderContext &context) :
  m_context(context), m_renderBuffer(nullptr)
{
}

bool CRenderBufferPoolFBO::IsCompatible(const CRenderVideoSettings &renderSettings) const
{
  return true;
}

IRenderBuffer *CRenderBufferPoolFBO::CreateRenderBuffer(void *header /* = nullptr */)
{
  if(m_eglContext == EGL_NO_CONTEXT)
  {
    if (!CreateContext())
      return nullptr;
  }

  return new CRenderBufferFBO(m_context);
}

bool CRenderBufferPoolFBO::CreateContext()
{
  auto winSystem =
      dynamic_cast<KODI::WINDOWING::LINUX::CWinSystemEGL*>(CServiceBroker::GetWinSystem());

  m_eglDisplay = winSystem->GetEGLDisplay();

  if (m_eglDisplay == EGL_NO_DISPLAY)
  {
    CLog::Log(LOGERROR, "failed to get EGL display");
    return false;
  }

  if (!eglInitialize(m_eglDisplay, NULL, NULL))
  {
    CLog::Log(LOGERROR, "failed to initialize EGL display");
    return false;
  }

  eglBindAPI(EGL_OPENGL_ES_API);

  EGLint attribs[] =
  {
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_RED_SIZE,   8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE,  8,
    EGL_ALPHA_SIZE, 8,
    EGL_DEPTH_SIZE, 16,
    EGL_NONE
  };

  EGLint neglconfigs;
  if (!eglChooseConfig(m_eglDisplay, attribs, &m_eglConfig, 1, &neglconfigs))
  {
    CLog::Log(LOGERROR, "Failed to query number of EGL configs");
    return false;
  }

  if (neglconfigs <= 0)
  {
    CLog::Log(LOGERROR, "No suitable EGL configs found");
    return false;
  }

  int client_version = 2;

  const EGLint context_attribs[] = {
    EGL_CONTEXT_CLIENT_VERSION, client_version, EGL_NONE
  };

  m_eglContext =
      eglCreateContext(m_eglDisplay, m_eglConfig, winSystem->GetEGLContext(), context_attribs);
  if (m_eglContext == EGL_NO_CONTEXT)
  {
    CLog::Log(LOGERROR, "failed to create EGL context");
    return false;
  }

  if (!eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, m_eglContext))
  {
    CLog::Log(LOGERROR, "Failed to make context current");
    return false;
  }

  CLog::Log(LOGDEBUG, "EGL CONTEXT SUCCESS");

  return true;
}

// IRenderBuffer* CRenderBufferPoolFBO::GetBuffer(unsigned int width, unsigned int height)
// {
//   if (!m_bConfigured)
//     return nullptr;

//   if (m_renderBuffer)
//     return m_renderBuffer.get();

//   CLog::Log(LOGDEBUG,
//             "RetroPlayer[RENDER]: Creating render buffer of size {}x{} for buffer pool", width,
//             height);

//   std::unique_ptr<CRenderBufferFBO> renderBufferPtr(static_cast<CRenderBufferFBO*>(CreateRenderBuffer(nullptr)));
//   if (renderBufferPtr->Allocate(m_format, width, height))
//     m_renderBuffer = std::move(renderBufferPtr);
//   else
//     CLog::Log(LOGERROR, "RetroPlayer[RENDER]: Failed to allocate render buffer");

//   if (m_renderBuffer)
//     m_renderBuffer->Acquire(GetPtr());

//   CLog::Log(LOGDEBUG, "GetBuffer(): {} fbo_id: {}", fmt::ptr(m_renderBuffer.get()),
//             m_renderBuffer->GetCurrentFramebuffer());

//   return m_renderBuffer.get();
// }

// void CRenderBufferPoolFBO::Return(IRenderBuffer* buffer)
// {
//   buffer->SetLoaded(false);
//   buffer->SetRendered(false);
// }
