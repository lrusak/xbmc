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
#pragma once

#include "cores/RetroPlayer/buffers/BaseRenderBufferPool.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "system_gl.h"

namespace KODI
{
namespace RETRO
{
  class CRenderBufferFBO;
  class IRenderBuffer;
  class CRenderContext;

  class CRenderBufferPoolFBO : public CBaseRenderBufferPool
  {
  public:
    CRenderBufferPoolFBO(CRenderContext &context);
    ~CRenderBufferPoolFBO() override = default;

    // implementation of IRenderBufferPool via CRenderBufferPoolSysMem
    bool IsCompatible(const CRenderVideoSettings &renderSettings) const override;

    // implementation of CBaseRenderBufferPool via CRenderBufferPoolSysMem
    IRenderBuffer *CreateRenderBuffer(void *header = nullptr) override;

    // IRenderBuffer* GetBuffer(unsigned int width, unsigned int height) override;
    // void Return(IRenderBuffer* buffer) override;

    bool Configure(uint32_t width, uint32_t height);

    uintptr_t GetCurrentFramebuffer() { return m_fbo_id; }

    void BindFramebuffer();
    void UnbindFramebuffer();

    bool IsConfigured2() const { return m_bConfigured2; }

  protected:
    bool CreateContext();

    // Construction parameters
    CRenderContext &m_context;

    // Configuration parameters
    EGLDisplay m_eglDisplay = EGL_NO_DISPLAY;
    EGLConfig m_eglConfig;
    EGLSurface m_eglSurface = EGL_NO_SURFACE;
    EGLContext m_eglContext = EGL_NO_CONTEXT;

  private:
    bool CreateFramebuffer();
    bool CreateRenderbuffer();
    bool CheckFrameBufferStatus();

    uint32_t m_width;
    uint32_t m_height;

    GLuint m_fbo_id;
    GLuint m_rbo_id;

    bool m_bConfigured2;
  };
}
}
