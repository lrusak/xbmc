/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <string>
#include <vector>

#include <vulkan/vulkan.hpp>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace KODI
{
namespace RENDERING
{
namespace VULKAN
{

struct Vertex;

class CVulkanDevice
{
public:
  CVulkanDevice();
  ~CVulkanDevice();

  void WaitIdle();

  void CreatePhysicalDevice(const vk::Instance& instance);
  void CreateLogicalDevice(const vk::SurfaceKHR& surface);
  void CreateSwapChain(const vk::SurfaceKHR& surface, const vk::Extent2D& extent);
  void CreateRenderPass();
  vk::UniqueRenderPass CreateUniqueRenderPass();

  void CreateFrameBuffer();

  void CreateCommandPool();
  vk::UniqueCommandPool CreateUniqueCommandPool();
  // void CreateTextureImage();
  std::tuple<vk::UniqueImage, vk::UniqueDeviceMemory> CreateUniqueTextureImage(
      const vk::Extent2D& extent);

  void CopyRegion(vk::Image& image,
                  const vk::Extent3D& imageExtent,
                  const vk::Extent3D& extent,
                  const vk::Offset3D& offset,
                  const vk::Format& format,
                  unsigned char* pixels,
                  uint32_t pitch);

  vk::UniqueImageView CreateUniqueTextureImageView(vk::Image& image);

  std::tuple<vk::UniqueBuffer, vk::UniqueDeviceMemory> CreateUniqueVertexBuffer(size_t size);
  std::tuple<vk::UniqueBuffer, vk::UniqueDeviceMemory> CreateUniqueIndexBuffer();
  void CreateUniformBuffers();

  std::vector<vk::UniqueCommandBuffer> CreateUniqueCommandBuffers();

  void CreateSyncObjects();

  void CleanupSwapChain();

  void Begin();
  void End();
  void Present();

  void AddCommandBuffers(std::vector<vk::UniqueCommandBuffer>& commandBuffers);

  bool ExtensionSupported(std::string extension);

  vk::Device& GetDevice() { return m_device.get(); }

  void DestroySwapChain();

  size_t GetSwapchainImagesSize() { return m_images.size(); }

  uint32_t GetImageIndex() { return m_imageIndex; }

  vk::Framebuffer& GetFramebuffer(size_t index) { return m_framebuffers[index].get(); }

  vk::Buffer& GetUniformBuffer(size_t index) { return m_uniformBuffers[index].get(); }

  vk::RenderPass& GetRenderPass() { return m_renderPass.get(); }
  // vk::Format& GetSurfaceFormat() { return m_surfaceFormat.format; }

  void UpdateVertexBuffer(vk::DeviceMemory& bufferMemory, const std::vector<Vertex>& vertices);
  void UpdateIndexBuffer(vk::DeviceMemory& bufferMemory, const std::vector<uint16_t>& indices);

private:
  uint32_t FindMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);

  std::tuple<vk::UniqueBuffer, vk::UniqueDeviceMemory> CreateBuffer(
      vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties);

  std::tuple<vk::UniqueImage, vk::UniqueDeviceMemory> CreateImage(
      uint32_t width,
      uint32_t height,
      vk::Format format,
      vk::ImageTiling tiling,
      vk::ImageUsageFlags usage,
      vk::MemoryPropertyFlags properties);

  vk::CommandBuffer BeginSingleTimeCommands();
  void EndSingleTimeCommands(vk::CommandBuffer commandBuffer);

  void CopyBufferToImage(vk::Buffer& buffer,
                         const vk::Extent3D& bufferExtent,
                         vk::Image& image,
                         const vk::Extent3D& extent,
                         const vk::Offset3D& offset);
  void CopyBuffer(vk::Buffer& srcBuffer, vk::Buffer& dstBuffer, vk::DeviceSize size);

  void TransitionImageLayout(vk::Image& image,
                             vk::Format format,
                             vk::ImageLayout oldLayout,
                             vk::ImageLayout newLayout);

  vk::UniqueImageView CreateImageView(vk::Image& image, vk::Format format);

  void UpdateUniformBuffer(uint32_t currentImage);

  vk::PhysicalDevice m_physicalDevice;
  vk::PhysicalDeviceProperties m_properties;

  std::vector<vk::ExtensionProperties> m_extensionProperties;
  std::vector<vk::QueueFamilyProperties> m_queueFamilyProperties;

  uint32_t m_graphicsQueueFamilyIndex;
  uint32_t m_presentQueueFamilyIndex;

  vk::UniqueDevice m_device;

  vk::Queue m_graphicsQueue;
  vk::Queue m_presentQueue;

  vk::SurfaceFormatKHR m_surfaceFormat;
  vk::SurfaceCapabilitiesKHR m_surfaceCapabilities;

  vk::UniqueSwapchainKHR m_swapChain;
  std::vector<vk::Image> m_images;
  std::vector<vk::UniqueImageView> m_imageViews;

  vk::UniqueRenderPass m_renderPass;

  std::vector<vk::UniqueFramebuffer> m_framebuffers;

  vk::UniqueCommandPool m_commandPool;

  std::vector<vk::CommandBuffer> m_buffers;

  std::vector<vk::UniqueFence> m_inFlightFences;
  std::vector<vk::Fence> m_imagesInFlightFences;
  std::vector<vk::UniqueSemaphore> m_imageAvailableSemaphores;
  std::vector<vk::UniqueSemaphore> m_renderFinishedSemaphores;

  std::vector<vk::UniqueBuffer> m_uniformBuffers;
  std::vector<vk::UniqueDeviceMemory> m_uniformBuffersMemory;

  vk::Extent2D m_extent;

  uint32_t m_imageIndex = 0;
  uint32_t m_currentFrame = 0;
};

} // namespace VULKAN
} // namespace RENDERING
} // namespace KODI
