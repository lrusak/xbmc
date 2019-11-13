/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "GUIFontTTF.h"
#include "TextureVulkan.h"
#include "rendering/vulkan/RenderSystemVulkan.h"
#include "rendering/vulkan/VertexBuffer.h"
#include "rendering/vulkan/VulkanDevice.h"

#include <string>
#include <vector>

class CGUIFontTTFVulkan : public CGUIFontTTF
{
public:
  explicit CGUIFontTTFVulkan(const std::string& strFileName);
  CGUIFontTTFVulkan() = delete;
  ~CGUIFontTTFVulkan() override;

  bool FirstBegin() override;
  void LastEnd() override;

  CVertexBuffer CreateVertexBuffer(const std::vector<SVertex>& vertices) const override;
  void DestroyVertexBuffer(CVertexBuffer& bufferHandle) const override;

protected:
  std::unique_ptr<CTexture> ReallocTexture(unsigned int& newHeight) override;
  bool CopyCharToTexture(FT_BitmapGlyph bitGlyph,
                         unsigned int x1,
                         unsigned int y1,
                         unsigned int x2,
                         unsigned int y2) override;
  void DeleteHardwareTexture() override;

private:
  static void AddVertex(CGUIFontTTFVulkan* font, KODI::RENDERING::VULKAN::Vertex& vertex);

  KODI::RENDERING::VULKAN::CRenderSystemVulkan* m_renderSystem;
  KODI::RENDERING::VULKAN::CVulkanDevice* m_device;

  vk::UniqueBuffer m_vertexBuffer;
  vk::UniqueDeviceMemory m_vertexBufferMemory;

  vk::UniqueBuffer m_indexBuffer;
  vk::UniqueDeviceMemory m_indexBufferMemory;

  vk::UniqueDescriptorPool m_descriptorPool;
  std::vector<vk::UniqueDescriptorSet> m_descriptorSets;

  std::vector<vk::UniqueCommandBuffer> m_commandBuffers;

  std::vector<KODI::RENDERING::VULKAN::Vertex> m_vertices;

  std::vector<uint16_t> m_indices = {0, 1, 2, 2, 3, 0};

  vk::UniqueImage m_image;
  vk::UniqueDeviceMemory m_deviceMemory;
  vk::UniqueImageView m_imageView;
};
