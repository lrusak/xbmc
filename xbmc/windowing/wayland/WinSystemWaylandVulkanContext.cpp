/*
 *  Copyright (C) 2017-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

// clang-format off
#define VK_USE_PLATFORM_WAYLAND_KHR
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_wayland.h>
// clang-format on

#include "WinSystemWaylandVulkanContext.h"

#include "utils/log.h"
#include "windowing/WindowSystemFactory.h"

using namespace KODI::WINDOWING::WAYLAND;

void CWinSystemWaylandVulkanContext::Register()
{
  CWindowSystemFactory::RegisterWindowSystem(CreateWinSystem, "wayland");
}

std::unique_ptr<CWinSystemBase> CWinSystemWaylandVulkanContext::CreateWinSystem()
{
  return std::make_unique<CWinSystemWaylandVulkanContext>();
}

CWinSystemWaylandVulkanContext::CWinSystemWaylandVulkanContext()
  : KODI::RENDERING::VULKAN::CRenderSystemVulkan()
{
  m_instanceExtensions.emplace_back(VK_KHR_SURFACE_EXTENSION_NAME);
  m_instanceExtensions.emplace_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
}

bool CWinSystemWaylandVulkanContext::InitWindowSystem()
{
  if (!CWinSystemWayland::InitWindowSystem())
    return false;

  if (!InitRenderSystem())
    return false;

  return true;
}

bool CWinSystemWaylandVulkanContext::DestroyWindowSystem()
{
  return CWinSystemWayland::DestroyWindowSystem();
}

bool CWinSystemWaylandVulkanContext::CreateNewWindow(const std::string& name,
                                                     bool fullScreen,
                                                     RESOLUTION_INFO& res)
{
  if (!CWinSystemWayland::CreateNewWindow(name, fullScreen, res))
    return false;

  if (!CreateSurface())
    return false;

  if (!ResetRenderSystem(res.iWidth, res.iHeight))
    return false;

  return true;
}

bool CWinSystemWaylandVulkanContext::DestroyWindow()
{
  DestroyRenderSystem();

  return CWinSystemWayland::DestroyWindow();
}

void CWinSystemWaylandVulkanContext::PresentRender(bool rendered, bool videoLayer)
{
  PrepareFramePresentation();

  if (rendered)
  {
    Present();
  }
  else
  {
    // For presentation feedback: Get notification of the next vblank even
    // when contents did not change
    GetMainSurface().commit();
    // Make sure it reaches the compositor
    GetConnection()->GetDisplay().flush();
  }

  FinishFramePresentation();

  if (m_sizeChanged)
  {
    CLog::LogF(LOGDEBUG, "Resetting render system to {}x{}", m_width, m_height);
    KODI::RENDERING::VULKAN::CRenderSystemVulkan::ResetRenderSystem(m_width, m_height);
    m_sizeChanged = false;
  }
}

void CWinSystemWaylandVulkanContext::SetContextSize(CSizeInt size)
{

  // Propagate changed dimensions to render system if necessary
  if (m_width != size.Width() || m_height != size.Height())
  {
    m_width = size.Width();
    m_height = size.Height();
    m_sizeChanged = true;
  }
}

bool CWinSystemWaylandVulkanContext::CreateSurface()
{
  m_nativeSurface = m_instance->createWaylandSurfaceKHRUnique(vk::WaylandSurfaceCreateInfoKHR(
      {}, this->GetConnection()->GetDisplay(), this->GetMainSurface()));
  if (!m_nativeSurface)
    return false;

  return true;
}
