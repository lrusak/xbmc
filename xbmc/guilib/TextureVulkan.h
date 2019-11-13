/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "Texture.h"
#include "rendering/vulkan/RenderSystemVulkan.h"
#include "rendering/vulkan/VulkanDevice.h"
#include "vulkan/vulkan.hpp"

class CTextureVulkan : public CTexture
{
public:
  CTextureVulkan(unsigned int width = 0,
                 unsigned int height = 0,
                 unsigned int format = XB_FMT_A8R8G8B8);
  CTextureVulkan(const CTextureVulkan& texture) = delete;
  ~CTextureVulkan() override;

  void CreateTextureObject() override {}
  void DestroyTextureObject() override {}
  void LoadToGPU() override;
  void BindToUnit(unsigned int unit) override {}

  vk::ImageView& GetImageView() { return m_imageView.get(); }

private:
  KODI::RENDERING::VULKAN::CRenderSystemVulkan* m_renderSystem;
  KODI::RENDERING::VULKAN::CVulkanDevice* m_device;

  vk::UniqueImage m_image;
  vk::UniqueDeviceMemory m_deviceMemory;
  vk::UniqueImageView m_imageView;

  vk::Format m_vkFormat;
};
