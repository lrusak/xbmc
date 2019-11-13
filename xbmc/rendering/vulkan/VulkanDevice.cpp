/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "VulkanDevice.h"

#include "UniformBuffer.h"
#include "Util.h"
#include "VertexBuffer.h"
#include "guilib/Texture.h"
#include "utils/log.h"

#include <set>

using namespace KODI::RENDERING::VULKAN;

CVulkanDevice::CVulkanDevice() = default;

CVulkanDevice::~CVulkanDevice()
{
  m_device->waitIdle();
}

void CVulkanDevice::WaitIdle()
{
  m_device->waitIdle();
}

void CVulkanDevice::CreatePhysicalDevice(const vk::Instance& instance)
{
  auto physicalDevices = instance.enumeratePhysicalDevices();

  //! @todo: improve gpu selection
  m_physicalDevice = physicalDevices.front();

  m_properties = m_physicalDevice.getProperties();

  CLog::Log(LOGDEBUG, "[VULKAN] API Version: {}.{}.{}", VK_VERSION_MAJOR(m_properties.apiVersion),
            VK_VERSION_MINOR(m_properties.apiVersion), VK_VERSION_PATCH(m_properties.apiVersion));
  CLog::Log(
      LOGDEBUG, "[VULKAN] Driver Version: {}.{}.{}", VK_VERSION_MAJOR(m_properties.driverVersion),
      VK_VERSION_MINOR(m_properties.driverVersion), VK_VERSION_PATCH(m_properties.driverVersion));
  CLog::Log(LOGDEBUG, "[VULKAN] Vendor ID: {:#4x}", m_properties.vendorID);
  CLog::Log(LOGDEBUG, "[VULKAN] Device ID: {:#4x}", m_properties.deviceID);
  CLog::Log(LOGDEBUG, "[VULKAN] Device Type: {}", vk::to_string(m_properties.deviceType));
  CLog::Log(LOGDEBUG, "[VULKAN] Device Name: {}", m_properties.deviceName);

  m_extensionProperties = m_physicalDevice.enumerateDeviceExtensionProperties();
}

void CVulkanDevice::CreateLogicalDevice(const vk::SurfaceKHR& surface)
{
  m_queueFamilyProperties = m_physicalDevice.getQueueFamilyProperties();

  m_graphicsQueueFamilyIndex =
      std::distance(m_queueFamilyProperties.begin(),
         std::find_if(m_queueFamilyProperties.begin(), m_queueFamilyProperties.end(),
                                 [](const vk::QueueFamilyProperties& qfp) {
                                   return qfp.queueFlags & vk::QueueFlagBits::eGraphics;
                                 }));

  for (size_t i = 0; i < m_queueFamilyProperties.size(); i++)
  {
    if (m_physicalDevice.getSurfaceSupportKHR(static_cast<uint32_t>(i), surface))
      m_presentQueueFamilyIndex = i;
  }

  std::vector<const char*> deviceExtensions;
  deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

  if (ExtensionSupported(VK_EXT_DEBUG_MARKER_EXTENSION_NAME))
    deviceExtensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);

  float queuePriority{0.0f};

  std::set<uint32_t> queueFamilyIndices = {m_graphicsQueueFamilyIndex, m_presentQueueFamilyIndex};

  std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
  for (const auto& queueFamilyIndex : queueFamilyIndices)
  {
    vk::DeviceQueueCreateInfo deviceQueueCreateInfo;
    deviceQueueCreateInfo.setQueueFamilyIndex(queueFamilyIndex).setQueuePriorities(queuePriority);

    queueCreateInfos.emplace_back(deviceQueueCreateInfo);
  }

  vk::DeviceCreateInfo deviceCreateInfo;
  deviceCreateInfo.setQueueCreateInfos(queueCreateInfos)
      .setPEnabledExtensionNames(deviceExtensions);

  m_device = m_physicalDevice.createDeviceUnique(deviceCreateInfo);

  m_graphicsQueue = m_device->getQueue(m_graphicsQueueFamilyIndex, 0);
  m_presentQueue = m_device->getQueue(m_presentQueueFamilyIndex, 0);
}

void CVulkanDevice::CreateSwapChain(const vk::SurfaceKHR& surface, const vk::Extent2D& extent)
{
  auto availableFormats = m_physicalDevice.getSurfaceFormatsKHR(surface);
  auto format =
      std::find_if(availableFormats.begin(), availableFormats.end(), [](auto& availableFormat) {
        return (availableFormat.format == vk::Format::eB8G8R8A8Srgb &&
                availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear);
      });

  if (format == availableFormats.end())
    throw std::invalid_argument("no format available");

  m_surfaceFormat.format = format->format;
  m_surfaceFormat.colorSpace = format->colorSpace;
  m_surfaceCapabilities = m_physicalDevice.getSurfaceCapabilitiesKHR(surface);

  vk::SharingMode sharingMode{vk::SharingMode::eExclusive};
  uint32_t indexCount{0};
  std::vector<uint32_t> queueFamilyIndices{m_graphicsQueueFamilyIndex, m_presentQueueFamilyIndex};

  if (m_graphicsQueueFamilyIndex != m_presentQueueFamilyIndex)
  {
    sharingMode = vk::SharingMode::eConcurrent;
    indexCount = 2;
  }

  vk::SwapchainCreateInfoKHR swapChainCreateInfo;
  swapChainCreateInfo.setSurface(surface)
      .setMinImageCount(m_surfaceCapabilities.minImageCount)
      .setImageFormat(m_surfaceFormat.format)
      .setImageColorSpace(m_surfaceFormat.colorSpace)
      .setImageExtent(extent)
      .setImageArrayLayers(1)
      .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
      .setImageSharingMode(sharingMode)
      .setQueueFamilyIndexCount(indexCount)
      .setQueueFamilyIndices(queueFamilyIndices)
      .setPreTransform(m_surfaceCapabilities.currentTransform)
      .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
      .setPresentMode(vk::PresentModeKHR::eMailbox)
      .setClipped(true)
      .setOldSwapchain(m_swapChain.get());

  m_swapChain = m_device->createSwapchainKHRUnique(swapChainCreateInfo);

  m_images = m_device->getSwapchainImagesKHR(m_swapChain.get());

  m_imageViews.clear();
  m_imageViews.reserve(m_images.size());

  vk::ComponentMapping componentMapping;
  componentMapping.setR(vk::ComponentSwizzle::eR)
      .setG(vk::ComponentSwizzle::eG)
      .setB(vk::ComponentSwizzle::eB)
      .setA(vk::ComponentSwizzle::eA);

  vk::ImageSubresourceRange subResourceRange;
  subResourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor)
      .setBaseMipLevel(0)
      .setLevelCount(1)
      .setBaseArrayLayer(0)
      .setLayerCount(1);

  for (auto image : m_images)
  {
    vk::ImageViewCreateInfo imageViewCreateInfo;
    imageViewCreateInfo.setImage(image)
        .setViewType(vk::ImageViewType::e2D)
        .setFormat(m_surfaceFormat.format)
        .setComponents(componentMapping)
        .setSubresourceRange(subResourceRange);

    m_imageViews.emplace_back(m_device->createImageViewUnique(imageViewCreateInfo));
  }

  m_extent = extent;
}

void CVulkanDevice::CleanupSwapChain()
{
  m_framebuffers.clear();
  m_renderPass.reset();
  m_imageViews.clear();
  m_uniformBuffers.clear();
  m_uniformBuffersMemory.clear();
}

void CVulkanDevice::CreateSyncObjects()
{
  if (m_imageAvailableSemaphores.size() == m_images.size())
    return;

  m_imagesInFlightFences.resize(m_images.size());

  vk::SemaphoreCreateInfo semaphoreCreateInfo;
  vk::FenceCreateInfo fenceCreateInfo(vk::FenceCreateFlags{VK_FENCE_CREATE_SIGNALED_BIT});

  for (size_t i = 0; i < m_images.size(); i++)
  {
    m_imageAvailableSemaphores.emplace_back(m_device->createSemaphoreUnique(semaphoreCreateInfo));
    m_renderFinishedSemaphores.emplace_back(m_device->createSemaphoreUnique(semaphoreCreateInfo));

    m_inFlightFences.emplace_back(m_device->createFenceUnique(fenceCreateInfo));
  }
}

void CVulkanDevice::CreateCommandPool()
{
  if (!m_commandPool)
    m_commandPool = CreateUniqueCommandPool();
}

vk::UniqueCommandPool CVulkanDevice::CreateUniqueCommandPool()
{
  vk::CommandPoolCreateInfo commandPoolCreateInfo;
  commandPoolCreateInfo.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
      .setQueueFamilyIndex(m_graphicsQueueFamilyIndex);

  return m_device->createCommandPoolUnique(commandPoolCreateInfo);
}

std::vector<vk::UniqueCommandBuffer> CVulkanDevice::CreateUniqueCommandBuffers()
{
  vk::CommandBufferAllocateInfo commandBufferAllocateInfo;
  commandBufferAllocateInfo.setCommandPool(m_commandPool.get())
      .setLevel(vk::CommandBufferLevel::ePrimary)
      .setCommandBufferCount(m_images.size());

  return m_device->allocateCommandBuffersUnique(commandBufferAllocateInfo);
}

void CVulkanDevice::AddCommandBuffers(std::vector<vk::UniqueCommandBuffer>& commandBuffers)
{
  m_buffers.emplace_back(commandBuffers[m_imageIndex].get());
}

void CVulkanDevice::CreateRenderPass()
{
  if (!m_renderPass)
    m_renderPass = CreateUniqueRenderPass();
}

vk::UniqueRenderPass CVulkanDevice::CreateUniqueRenderPass()
{
  vk::AttachmentDescription attachmentDescription;
  attachmentDescription.setFormat(m_surfaceFormat.format)
      .setSamples(vk::SampleCountFlagBits::e1)
      .setLoadOp(vk::AttachmentLoadOp::eDontCare)
      .setStoreOp(vk::AttachmentStoreOp::eStore)
      .setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

  vk::AttachmentReference attachmentReference;
  attachmentReference.setAttachment(0).setLayout(vk::ImageLayout::eColorAttachmentOptimal);

  vk::SubpassDescription subpassDescription;
  subpassDescription.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
      .setColorAttachments(attachmentReference);

  vk::SubpassDependency subpassDependency;
  subpassDependency.setSrcSubpass(VK_SUBPASS_EXTERNAL)
      .setDstSubpass(0)
      .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
      .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
      .setSrcAccessMask({})
      .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

  vk::RenderPassCreateInfo renderPassCreateInfo;
  renderPassCreateInfo.setAttachments(attachmentDescription)
      .setSubpasses(subpassDescription)
      .setDependencies(subpassDependency);

  return m_device->createRenderPassUnique(renderPassCreateInfo);
}

void CVulkanDevice::CreateFrameBuffer()
{
  auto imageViews = vk::uniqueToRaw(m_imageViews);

  m_framebuffers.clear();
  for (size_t i = 0; i < imageViews.size(); i++)
  {
    vk::FramebufferCreateInfo framebufferCreateInfo;
    framebufferCreateInfo.setRenderPass(m_renderPass.get())
        .setAttachments(imageViews[i])
        .setWidth(m_extent.width)
        .setHeight(m_extent.height)
        .setLayers(1);

    m_framebuffers.emplace_back(m_device->createFramebufferUnique(framebufferCreateInfo));
  }
}

void CVulkanDevice::CopyBuffer(vk::Buffer& srcBuffer, vk::Buffer& dstBuffer, vk::DeviceSize size)
{
  auto commandBuffer = BeginSingleTimeCommands();

  vk::BufferCopy bufferCopy(0, 0, size);
  commandBuffer.copyBuffer(srcBuffer, dstBuffer, bufferCopy);
  EndSingleTimeCommands(commandBuffer);
}

std::tuple<vk::UniqueBuffer, vk::UniqueDeviceMemory> CVulkanDevice::CreateUniqueVertexBuffer(
    size_t size)
{
  vk::DeviceSize bufferSize = sizeof(Vertex) * size;

  vk::UniqueBuffer buffer;
  vk::UniqueDeviceMemory deviceMemory;

  std::tie(buffer, deviceMemory) = CreateBuffer(bufferSize, vk::BufferUsageFlagBits::eVertexBuffer,
                                                vk::MemoryPropertyFlagBits::eHostVisible |
                                                    vk::MemoryPropertyFlagBits::eHostCoherent);

  return std::make_tuple(std::move(buffer), std::move(deviceMemory));
}

void CVulkanDevice::UpdateVertexBuffer(vk::DeviceMemory& bufferMemory,
                                       const std::vector<Vertex>& vertices)
{
  vk::DeviceSize bufferSize = sizeof(Vertex) * vertices.size();

  auto data = m_device->mapMemory(bufferMemory, 0, bufferSize);
  std::memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
  m_device->unmapMemory(bufferMemory);
}

std::tuple<vk::UniqueBuffer, vk::UniqueDeviceMemory> CVulkanDevice::CreateUniqueIndexBuffer()
{
  vk::DeviceSize bufferSize = sizeof(uint16_t) * 6; //indices.size();

  vk::UniqueBuffer buffer;
  vk::UniqueDeviceMemory deviceMemory;

  std::tie(buffer, deviceMemory) = CreateBuffer(bufferSize, vk::BufferUsageFlagBits::eIndexBuffer,
                                                vk::MemoryPropertyFlagBits::eHostVisible |
                                                    vk::MemoryPropertyFlagBits::eHostCoherent);

  return std::make_tuple(std::move(buffer), std::move(deviceMemory));
}

void CVulkanDevice::UpdateIndexBuffer(vk::DeviceMemory& bufferMemory,
                                      const std::vector<uint16_t>& indices)
{
  vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

  auto data = m_device->mapMemory(bufferMemory, 0, bufferSize);
  std::memcpy(data, indices.data(), static_cast<size_t>(bufferSize));
  m_device->unmapMemory(bufferMemory);
}

void CVulkanDevice::CreateUniformBuffers()
{
  if (m_uniformBuffers.size() > 0)
    return;

  vk::DeviceSize bufferSize = sizeof(UniformBufferObject);

  m_uniformBuffers.resize(m_images.size());
  m_uniformBuffersMemory.resize(m_images.size());

  for (size_t i = 0; i < m_images.size(); i++)
    std::tie(m_uniformBuffers[i], m_uniformBuffersMemory[i]) = CreateBuffer(
        bufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
}

bool CVulkanDevice::ExtensionSupported(std::string extension)
{
  return (std::find_if(m_extensionProperties.begin(), m_extensionProperties.end(),
                       [&extension](auto p) { return p.extensionName.data() == extension; }) !=
          m_extensionProperties.end());
}

void CVulkanDevice::UpdateUniformBuffer(uint32_t currentImage)
{
  UniformBufferObject ubo{};

  glm::identity<glm::mat4>();

  ubo.model = glm::identity<glm::mat4>();
  ubo.view = glm::identity<glm::mat4>();
  ubo.proj = glm::identity<glm::mat4>();

  ubo.proj = glm::orthoLH(0.0f, m_extent.width - 1.0f, 0.0f, m_extent.height - 1.0f, -1.0f, 1.0f);

  // CLog::Log(LOGDEBUG, "CVulkanDevice::{} - ubo:", __FUNCTION__);
  // CLog::Log(LOGDEBUG, "CVulkanDevice::{} -   model:", __FUNCTION__);
  // for (int i = 0; i < 4; i++)
  //   CLog::Log(LOGDEBUG, "CVulkanDevice::{} -     row {}: [{}][{}][{}][{}]", __FUNCTION__, i,
  //             ubo.model[i][0], ubo.model[i][1], ubo.model[i][2], ubo.model[i][3]);

  // CLog::Log(LOGDEBUG, "CVulkanDevice::{} -   view:", __FUNCTION__);
  // for (int i = 0; i < 4; i++)
  //   CLog::Log(LOGDEBUG, "CVulkanDevice::{} -     row {}: [{}][{}][{}][{}]", __FUNCTION__, i,
  //             ubo.view[i][0], ubo.view[i][1], ubo.view[i][2], ubo.view[i][3]);

  // CLog::Log(LOGDEBUG, "CVulkanDevice::{} -   proj:", __FUNCTION__);
  // for (int i = 0; i < 4; i++)
  //   CLog::Log(LOGDEBUG, "CVulkanDevice::{} -     row {}: [{}][{}][{}][{}]", __FUNCTION__, i,
  //             ubo.proj[i][0], ubo.proj[i][1], ubo.proj[i][2], ubo.proj[i][3]);

  auto data = m_device->mapMemory(m_uniformBuffersMemory[currentImage].get(), 0, sizeof(ubo));
  std::memcpy(data, &ubo, sizeof(ubo));
  m_device->unmapMemory(m_uniformBuffersMemory[currentImage].get());
}

void CVulkanDevice::Begin()
{
  m_device->waitForFences(m_inFlightFences[m_currentFrame].get(), VK_TRUE,
                          std::numeric_limits<uint64_t>::max());

  auto result =
      m_device->acquireNextImageKHR(m_swapChain.get(), std::numeric_limits<uint64_t>::max(),
                                    m_imageAvailableSemaphores[m_currentFrame].get(), {});

  m_imageIndex = result.value;

  UpdateUniformBuffer(m_imageIndex);
}

void CVulkanDevice::End()
{
  if (m_imagesInFlightFences[m_imageIndex])
    m_device->waitForFences(m_imagesInFlightFences[m_imageIndex], VK_TRUE,
                            std::numeric_limits<uint64_t>::max());

  m_imagesInFlightFences[m_imageIndex] = m_inFlightFences[m_currentFrame].get();

  vk::PipelineStageFlags pipelineStageFlags(vk::PipelineStageFlagBits::eColorAttachmentOutput);

  vk::SubmitInfo submitInfo;
  submitInfo.setWaitSemaphores(m_imageAvailableSemaphores[m_currentFrame].get())
      .setWaitDstStageMask(pipelineStageFlags)
      .setCommandBuffers(m_buffers)
      .setSignalSemaphores(m_renderFinishedSemaphores[m_imageIndex].get());

  m_device->resetFences(m_inFlightFences[m_currentFrame].get());

  m_graphicsQueue.submit(submitInfo, m_inFlightFences[m_currentFrame].get());
}

void CVulkanDevice::Present()
{
  vk::PresentInfoKHR presentInfo;
  presentInfo.setWaitSemaphores(m_renderFinishedSemaphores[m_imageIndex].get())
      .setSwapchains(m_swapChain.get())
      .setImageIndices(m_imageIndex);

  m_presentQueue.presentKHR(presentInfo);

  m_currentFrame = (m_currentFrame + 1) % 2;

  m_presentQueue.waitIdle();
  m_device->waitIdle();
  m_buffers.clear();
}

uint32_t CVulkanDevice::FindMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
{
  auto phyicalDeviceMemoryProperties = m_physicalDevice.getMemoryProperties();

  for (uint32_t i = 0; i < phyicalDeviceMemoryProperties.memoryTypeCount; i++)
  {
    if ((typeFilter & (1 << i)) &&
        (phyicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
      return i;
  }

  throw std::runtime_error("failed to find memory type");
}

std::tuple<vk::UniqueBuffer, vk::UniqueDeviceMemory> CVulkanDevice::CreateBuffer(
    vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties)
{
  vk::BufferCreateInfo bufferCreateInfo;
  bufferCreateInfo.setSize(size).setUsage(usage).setSharingMode(vk::SharingMode::eExclusive);

  auto buffer = m_device->createBufferUnique(bufferCreateInfo);

  auto memoryRequirements = m_device->getBufferMemoryRequirements(buffer.get());

  vk::MemoryAllocateInfo memoryAllocateInfo;
  memoryAllocateInfo.setAllocationSize(memoryRequirements.size)
      .setMemoryTypeIndex(FindMemoryType(memoryRequirements.memoryTypeBits, properties));

  auto bufferMemory = m_device->allocateMemoryUnique(memoryAllocateInfo);

  m_device->bindBufferMemory(buffer.get(), bufferMemory.get(), 0);

  return std::make_tuple(std::move(buffer), std::move(bufferMemory));
}

std::tuple<vk::UniqueImage, vk::UniqueDeviceMemory> CVulkanDevice::CreateImage(
    uint32_t width,
    uint32_t height,
    vk::Format format,
    vk::ImageTiling tiling,
    vk::ImageUsageFlags usage,
    vk::MemoryPropertyFlags properties)
{
  vk::ImageCreateInfo imageCreateInfo;
  imageCreateInfo.setImageType(vk::ImageType::e2D)
      .setFormat(format)
      .setExtent(vk::Extent3D(width, height, 1))
      .setMipLevels(1)
      .setArrayLayers(1)
      .setSamples(vk::SampleCountFlagBits::e1)
      .setTiling(tiling)
      .setUsage(usage)
      .setSharingMode(vk::SharingMode::eExclusive)
      .setInitialLayout(vk::ImageLayout::eUndefined);

  auto image = m_device->createImageUnique(imageCreateInfo);

  auto memoryRequirements = m_device->getImageMemoryRequirements(image.get());

  vk::MemoryAllocateInfo memoryAllocateInfo;
  memoryAllocateInfo.setAllocationSize(memoryRequirements.size)
      .setMemoryTypeIndex(FindMemoryType(memoryRequirements.memoryTypeBits, properties));

  auto imageMemory = m_device->allocateMemoryUnique(memoryAllocateInfo);

  m_device->bindImageMemory(image.get(), imageMemory.get(), 0);

  return std::make_tuple(std::move(image), std::move(imageMemory));
}

vk::CommandBuffer CVulkanDevice::BeginSingleTimeCommands()
{
  vk::CommandBufferAllocateInfo commandBufferAllocateInfo;
  commandBufferAllocateInfo.setCommandPool(m_commandPool.get())
      .setLevel(vk::CommandBufferLevel::ePrimary)
      .setCommandBufferCount(1);

  auto commandBuffers = m_device->allocateCommandBuffers(commandBufferAllocateInfo);

  vk::CommandBufferBeginInfo commandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

  commandBuffers[0].begin(commandBufferBeginInfo);

  return commandBuffers[0];
}

void CVulkanDevice::EndSingleTimeCommands(vk::CommandBuffer commandBuffer)
{
  commandBuffer.end();

  vk::SubmitInfo submitInfo;
  submitInfo.setCommandBuffers(commandBuffer);

  m_graphicsQueue.submit(submitInfo, vk::Fence());
  m_graphicsQueue.waitIdle();

  m_device->freeCommandBuffers(m_commandPool.get(), commandBuffer);
}

void CVulkanDevice::TransitionImageLayout(vk::Image& image,
                                          vk::Format format,
                                          vk::ImageLayout oldLayout,
                                          vk::ImageLayout newLayout)
{
  vk::ImageSubresourceRange subResourceRange;
  subResourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor)
      .setBaseMipLevel(0)
      .setLevelCount(1)
      .setBaseArrayLayer(0)
      .setLayerCount(1);

  vk::ImageMemoryBarrier imageMemoryBarrier;
  imageMemoryBarrier.setOldLayout(oldLayout)
      .setNewLayout(newLayout)
      .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
      .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
      .setImage(image)
      .setSubresourceRange(subResourceRange);

  vk::PipelineStageFlags sourceStage;
  vk::PipelineStageFlags destinationStage;

  if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal)
  {
    imageMemoryBarrier.setSrcAccessMask(vk::AccessFlags());
    imageMemoryBarrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);

    sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
    destinationStage = vk::PipelineStageFlagBits::eTransfer;
  }
  else if (oldLayout == vk::ImageLayout::eTransferDstOptimal &&
           newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
  {
    imageMemoryBarrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
    imageMemoryBarrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);

    sourceStage = vk::PipelineStageFlagBits::eTransfer;
    destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
  }
  else
    throw std::invalid_argument("unimplemented");

  auto commandBuffer = BeginSingleTimeCommands();

  commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, nullptr, nullptr,
                                imageMemoryBarrier);

  EndSingleTimeCommands(commandBuffer);
}

void CVulkanDevice::CopyBufferToImage(vk::Buffer& buffer,
                                      const vk::Extent3D& bufferExtent,
                                      vk::Image& image,
                                      const vk::Extent3D& extent,
                                      const vk::Offset3D& offset)
{
  auto commandBuffer = BeginSingleTimeCommands();

  vk::ImageSubresourceLayers subResouceLayers;
  subResouceLayers.setAspectMask(vk::ImageAspectFlagBits::eColor)
      .setMipLevel(0)
      .setBaseArrayLayer(0)
      .setLayerCount(1);

  vk::BufferImageCopy bufferImageCopy;
  bufferImageCopy.setImageSubresource(subResouceLayers)
      .setImageOffset(vk::Offset3D(0, 0, 0))
      .setImageExtent(extent)
      .setBufferRowLength(extent.width)
      .setBufferImageHeight(bufferExtent.height);

  commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal,
                                  bufferImageCopy);

  EndSingleTimeCommands(commandBuffer);
}

std::tuple<vk::UniqueImage, vk::UniqueDeviceMemory> CVulkanDevice::CreateUniqueTextureImage(
    const vk::Extent2D& extent)
{
  vk::UniqueImage textureImage;
  vk::UniqueDeviceMemory textureImageMemory;

  std::tie(textureImage, textureImageMemory) =
      CreateImage(extent.width, extent.height, m_surfaceFormat.format, vk::ImageTiling::eOptimal,
                  vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                  vk::MemoryPropertyFlagBits::eDeviceLocal);

  return std::make_tuple(std::move(textureImage), std::move(textureImageMemory));
}

std::map<vk::Format, uint8_t> vulkanFormatSize = {
    {vk::Format::eR8Snorm, 1},
    {vk::Format::eR8G8B8Snorm, 3},
    {vk::Format::eR8G8B8A8Snorm, 4},
};

void CVulkanDevice::CopyRegion(vk::Image& image,
                               const vk::Extent3D& imageExtent,
                               const vk::Extent3D& extent,
                               const vk::Offset3D& offset,
                               const vk::Format& format,
                               unsigned char* pixels,
                               uint32_t pitch)
{
  vk::DeviceSize imageSize = imageExtent.width * imageExtent.height * vulkanFormatSize[format];

  vk::UniqueBuffer stagingBuffer;
  vk::UniqueDeviceMemory stagingBufferMemory;

  std::tie(stagingBuffer, stagingBufferMemory) = CreateBuffer(
      imageSize, vk::BufferUsageFlagBits::eTransferSrc,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

  auto data = m_device->mapMemory(stagingBufferMemory.get(), 0, imageSize, {});

  uint8_t* source = pixels;
  uint8_t* target = static_cast<uint8_t*>(data) +
                    offset.y * imageExtent.width * vulkanFormatSize[format] + offset.x;

  for (uint32_t y = offset.y; y < extent.height; y++)
  {
    std::memcpy(target, source, (extent.width * vulkanFormatSize[format] - offset.x));
    source += pitch * vulkanFormatSize[format];
    target += imageExtent.width * vulkanFormatSize[format];
  }

  // std::memcpy(data, pixels, static_cast<size_t>(imageSize));
  m_device->unmapMemory(stagingBufferMemory.get());

  //! @todo: do we care about other original texture formats?
  TransitionImageLayout(image, vk::Format::eB8G8R8A8Srgb, vk::ImageLayout::eUndefined,
                        vk::ImageLayout::eTransferDstOptimal);

  CopyBufferToImage(
      stagingBuffer.get(),
      vk::Extent3D(imageExtent.width * vulkanFormatSize[format], imageExtent.height, 1), image,
      imageExtent, offset);

  TransitionImageLayout(image, vk::Format::eB8G8R8A8Srgb, vk::ImageLayout::eTransferDstOptimal,
                        vk::ImageLayout::eShaderReadOnlyOptimal);
}

vk::UniqueImageView CVulkanDevice::CreateImageView(vk::Image& image, vk::Format format)
{
  vk::ImageSubresourceRange subResourceRange;
  subResourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor)
      .setBaseMipLevel(0)
      .setLevelCount(1)
      .setBaseArrayLayer(0)
      .setLayerCount(1);

  vk::ImageViewCreateInfo imageViewCreateInfo;
  imageViewCreateInfo.setImage(image)
      .setViewType(vk::ImageViewType::e2D)
      .setFormat(format)
      .setSubresourceRange(subResourceRange);

  auto imageView = m_device->createImageViewUnique(imageViewCreateInfo);

  return imageView;
}

vk::UniqueImageView CVulkanDevice::CreateUniqueTextureImageView(vk::Image& image)
{
  return CreateImageView(image, m_surfaceFormat.format);
}

void CVulkanDevice::DestroySwapChain()
{
  m_swapChain.reset();
}
