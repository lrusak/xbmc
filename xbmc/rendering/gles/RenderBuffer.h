/*
 *  Copyright (C) 2005-2021 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <list>
#include <memory>
#include <stdint.h>

#include "system_gl.h"

class CRenderBufferPool;

class CRenderBuffer : public std::enable_shared_from_this<CRenderBuffer>
{
public:
  CRenderBuffer();
  ~CRenderBuffer();

  bool Allocate(uint32_t width, uint32_t height);
  void Release();

  void BindFrameBuffer();
  void UnbindFrameBuffer();

  void BindTexture();
  void UnbindTexture();

  uint32_t GetFrameBufferID() const { return m_fboid; }

  bool IsCompatible(uint32_t width, uint32_t height);

  bool Render();
  bool RenderCube();

  void SetPool(std::shared_ptr<CRenderBufferPool> pool) { m_pool = pool; }

private:
  GLuint m_fboid{0};
  GLuint m_texid{0};

  uint32_t m_width{0};
  uint32_t m_height{0};

  std::shared_ptr<CRenderBufferPool> m_pool;

  uint32_t m_i{0};
};

class CRenderBufferPool : public std::enable_shared_from_this<CRenderBufferPool>
{
public:
  CRenderBufferPool();
  ~CRenderBufferPool();

  std::shared_ptr<CRenderBuffer> GetBuffer(uint32_t width, uint32_t height);
  void Return(std::shared_ptr<CRenderBuffer> buffer);

private:
  std::list<std::shared_ptr<CRenderBuffer>> m_used;
  std::list<std::shared_ptr<CRenderBuffer>> m_free;
};
