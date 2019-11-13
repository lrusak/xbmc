/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "VulkanDebug.h"
#include "VulkanDevice.h"
#include "VulkanShader.h"
#include "VulkanShaders.h"
#include "rendering/RenderSystem.h"
#include "utils/ColorUtils.h"

#include <array>
#include <memory>
#include <vector>

namespace KODI
{
namespace RENDERING
{
namespace VULKAN
{

class CRenderSystemVulkan : public CRenderSystemBase
{
public:
  CRenderSystemVulkan();
  ~CRenderSystemVulkan() = default;

  bool InitRenderSystem() override;
  bool DestroyRenderSystem() override;
  bool ResetRenderSystem(int width, int height) override;

  bool BeginRender() override;
  bool EndRender() override;

  bool Present();

  bool ClearBuffers(UTILS::COLOR::Color color) override;
  bool IsExtSupported(const char* extension) const override;

  void SetViewPort(const CRect& viewPort);
  void GetViewPort(CRect& viewPort);

  void SetScissors(const CRect& rect);
  void ResetScissors() override;

  void CaptureStateBlock() override;
  void ApplyStateBlock() override;
  void SetCameraPosition(const CPoint& camera,
                         int screenWidth,
                         int screenHeight,
                         float stereoFactor = 0.f) override;

  void DestroySwapChain();

  CVulkanDevice* GetDevice() const { return m_device.get(); }

  CVulkanShader* GetShader(VULKANSHADER method);

protected:
  virtual bool CreateSurface() = 0;

  int m_width;
  int m_height;

  std::vector<const char*> m_instanceExtensions;

  vk::UniqueInstance m_instance;
  vk::UniqueSurfaceKHR m_nativeSurface;

private:
  std::array<std::unique_ptr<CVulkanShader>, static_cast<size_t>(VULKANSHADER::MAX)> m_shaders;

  bool m_initialized{false};

  std::unique_ptr<CVulkanDebug> m_debug;
  std::unique_ptr<CVulkanDevice> m_device;
};

} // namespace VULKAN
} // namespace RENDERING
} // namespace KODI
