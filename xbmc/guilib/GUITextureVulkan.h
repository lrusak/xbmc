/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "GUITexture.h"
#include "rendering/vulkan/RenderSystemVulkan.h"
#include "rendering/vulkan/VertexBuffer.h"
#include "rendering/vulkan/VulkanDevice.h"
#include "rendering/vulkan/VulkanShaders.h"
#include "utils/ColorUtils.h"
#include "vulkan/vulkan.hpp"

class CGUITextureVulkan : public CGUITexture
{
public:
  static void Register();
  static CGUITexture* CreateTexture(
      float posX, float posY, float width, float height, const CTextureInfo& texture);

  static void DrawQuad(const CRect& coords,
                       UTILS::COLOR::Color color,
                       CTexture* texture = nullptr,
                       const CRect* texCoords = nullptr);

  CGUITextureVulkan(float posX, float posY, float width, float height, const CTextureInfo& texture);
  ~CGUITextureVulkan() override;

  CGUITextureVulkan* Clone() const override;

protected:
  void Begin(UTILS::COLOR::Color color) override;
  void Draw(float* x,
            float* y,
            float* z,
            const CRect& texture,
            const CRect& diffuse,
            int orientation) override;
  void End() override;

private:
  CGUITextureVulkan(const CGUITextureVulkan& texture);

  KODI::RENDERING::VULKAN::CRenderSystemVulkan* m_renderSystem;
  KODI::RENDERING::VULKAN::CVulkanDevice* m_device;

  KODI::RENDERING::VULKAN::VULKANSHADER m_shaderMethod;

  vk::UniqueBuffer m_vertexBuffer;
  vk::UniqueDeviceMemory m_vertexBufferMemory;

  vk::UniqueBuffer m_indexBuffer;
  vk::UniqueDeviceMemory m_indexBufferMemory;

  vk::UniqueDescriptorPool m_descriptorPool;
  std::vector<vk::UniqueDescriptorSet> m_descriptorSets;

  std::vector<vk::UniqueCommandBuffer> m_commandBuffers;

  std::vector<KODI::RENDERING::VULKAN::Vertex> m_vertices;

  std::vector<uint16_t> m_indices = {0, 1, 2, 2, 3, 0};
};
