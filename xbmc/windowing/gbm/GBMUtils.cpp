/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GBMUtils.h"

#include "utils/log.h"

using namespace KODI::WINDOWING::GBM;

bool CGBMUtils::CreateDevice(int fd)
{
  auto device = gbm_create_device(fd);
  if (!device)
  {
    CLog::Log(LOGERROR, "CGBMUtils::{} - failed to create device: {}", __FUNCTION__,
              strerror(errno));
    return false;
  }

  m_device.reset(new CGBMDevice(device));

  return true;
}

CGBMUtils::CGBMDevice::CGBMDevice(gbm_device* device) : m_device(device)
{
}

bool CGBMUtils::CGBMDevice::CreateSurface(
    int width, int height, uint32_t format, const uint64_t* modifiers, const int modifiers_count)
{
  gbm_surface* surface{nullptr};
#if defined(HAS_GBM_MODIFIERS)
  if (modifiers)
  {
    surface = gbm_surface_create_with_modifiers(m_device, width, height, format, modifiers,
                                                modifiers_count);
  }
#endif
  if (!surface)
  {
    surface = gbm_surface_create(m_device, width, height, format,
                                 GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
  }

  if (!surface)
  {
    CLog::Log(LOGERROR, "CGBMUtils::{} - failed to create surface: {}", __FUNCTION__,
              strerror(errno));
    return false;
  }

  CLog::Log(LOGDEBUG, "CGBMUtils::{} - created surface with size {}x{}", __FUNCTION__, width,
            height);

  m_surface.reset(new CGBMSurface(surface));

  return true;
}

CGBMUtils::CGBMDevice::CGBMSurface::CGBMSurface(gbm_surface* surface)
  : m_lastupdate(std::chrono::steady_clock::now()),
    m_surface(surface),
    m_front_buffer(std::make_unique<CGBMSurfaceBuffer>(surface)),
    m_back_buffer(std::make_unique<CGBMSurfaceBuffer>(surface))
{
}

CGBMUtils::CGBMDevice::CGBMSurface::CGBMSurfaceBuffer* CGBMUtils::CGBMDevice::CGBMSurface::
    LockFrontBuffer()
{
  const auto now = std::chrono::steady_clock::now();
  const auto diff = now - m_lastupdate;
  CLog::Log(LOGDEBUG, "diff: {:.3f} ms",
            std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(diff).count());
  m_lastupdate = now;

  std::swap(m_front_buffer, m_back_buffer);

  m_front_buffer->Lock();

  m_back_buffer->Release();

  return m_front_buffer.get();
}

CGBMUtils::CGBMDevice::CGBMSurface::CGBMSurfaceBuffer::CGBMSurfaceBuffer(gbm_surface* surface)
  : m_surface(surface), m_buffer(nullptr)
{
}

CGBMUtils::CGBMDevice::CGBMSurface::CGBMSurfaceBuffer::~CGBMSurfaceBuffer()
{
  Release();
}

void CGBMUtils::CGBMDevice::CGBMSurface::CGBMSurfaceBuffer::Lock()
{
  m_buffer = gbm_surface_lock_front_buffer(m_surface);
}

void CGBMUtils::CGBMDevice::CGBMSurface::CGBMSurfaceBuffer::Release()
{
  if (m_surface && m_buffer)
    gbm_surface_release_buffer(m_surface, m_buffer);
}
