/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "VulkanShaders.h"

#include <vector>

#include <vulkan/vulkan.hpp>

namespace KODI
{
namespace RENDERING
{
namespace VULKAN
{

class CVulkanDevice;

class CVulkanShader
{
public:
  explicit CVulkanShader(CVulkanDevice* device, VULKANSHADER method);
  CVulkanShader() = delete;
  ~CVulkanShader() = default;

  bool InitializeShader(vk::Extent2D extent);

  vk::UniqueDescriptorPool CreateUniqueDescriptorPool();

  std::vector<vk::UniqueDescriptorSet> CreateUniqueDescriptorSets(
      vk::UniqueDescriptorPool& descriptorPool);

  void UpdateDescriptorWrites(std::vector<vk::UniqueDescriptorSet>& descriptorSets,
                              vk::ImageView& imageView);

  void UpdateCommandBuffers(std::vector<vk::UniqueCommandBuffer>& commandBuffers,
                            std::vector<vk::UniqueDescriptorSet>& descriptorSets,
                            vk::Buffer& vertexBuffer,
                            vk::Buffer& indexBuffer);

  void UpdatePipeline(vk::Extent2D extent);

  void SetAlpha(bool alpha) { m_alpha = alpha; }

private:
  void CreateUniqueDescriptorLayout();
  void CreateUniquePipelineLayout();
  void CreateUniqueVertexShaderModule();
  void CreateUniqueFragmentShaderModule();
  void CreateUniquePipeline(vk::Extent2D extent);
  void CreateUniqueTextureSampler();

  CVulkanDevice* m_device;
  VULKANSHADER m_method;

  bool m_alpha = false;

  vk::UniqueDescriptorSetLayout m_descriptorSetLayout;

  vk::UniquePipelineLayout m_pipelineLayout;

  vk::UniqueShaderModule m_vertShaderModule;
  vk::UniqueShaderModule m_fragShaderModule;

  vk::UniquePipeline m_pipeline;
  vk::UniquePipeline m_pipelineAlpha;

  vk::UniqueCommandPool m_commandPool;

  vk::UniqueSampler m_sampler;

  vk::Extent2D m_extent;
};


} // namespace VULKAN
} // namespace RENDERING
} // namespace KODI
