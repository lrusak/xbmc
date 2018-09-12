/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GBMUtils.h"

#include "utils/log.h"

#include <mutex>

using namespace KODI::WINDOWING::GBM;

namespace
{
std::once_flag flag;
}

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

CGBMUtils::CGBMDevice::CGBMSurface::CGBMSurface(gbm_surface* surface) : m_surface(surface)
{
}

CGBMUtils::CGBMDevice::CGBMSurface::CGBMSurfaceBuffer* CGBMUtils::CGBMDevice::CGBMSurface::
    LockFrontBuffer()
{
  m_buffers.emplace(std::make_unique<CGBMSurfaceBuffer>(m_surface));

  if (!static_cast<bool>(gbm_surface_has_free_buffers(m_surface)))
  {
    /*
     * We want to use call_once here because we want it to be logged the first time that
     * we have to release buffers. This means that the maximum amount of buffers had been reached.
     * For mesa this should be 4 buffers but it may vary accross other implementations.
     */
    std::call_once(
        flag, [this]() { CLog::Log(LOGDEBUG, "CGBMUtils - using {} buffers", m_buffers.size()); });

    m_buffers.pop();
  }

  return m_buffers.back().get();
}

CGBMUtils::CGBMDevice::CGBMSurface::CGBMSurfaceBuffer::CGBMSurfaceBuffer(gbm_surface* surface)
  : m_surface(surface), m_buffer(gbm_surface_lock_front_buffer(surface))
{
}

CGBMUtils::CGBMDevice::CGBMSurface::CGBMSurfaceBuffer::~CGBMSurfaceBuffer()
{
  ReleaseMemory();

  if (m_surface && m_buffer)
    gbm_surface_release_buffer(m_surface, m_buffer);
}

uint8_t* CGBMUtils::CGBMDevice::CGBMSurface::CGBMSurfaceBuffer::GetMemory()
{
  if (m_buffer)
  {
    m_map = static_cast<uint8_t*>(gbm_bo_map(m_buffer, 0, 0, gbm_bo_get_width(m_buffer),
                                             gbm_bo_get_height(m_buffer), GBM_BO_TRANSFER_READ,
                                             &m_stride, &m_map_data));
    if (m_map)
      return m_map;
  }

  return nullptr;
}

void CGBMUtils::CGBMDevice::CGBMSurface::CGBMSurfaceBuffer::ReleaseMemory()
{
  if (m_buffer && m_map)
  {
    gbm_bo_unmap(m_buffer, m_map_data);
    m_map_data = nullptr;
    m_map = nullptr;
  }
}
