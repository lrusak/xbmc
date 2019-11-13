/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "VulkanShader.h"

#include "UniformBuffer.h"
#include "VertexBuffer.h"
#include "VulkanDevice.h"

#include <map>

using namespace KODI::RENDERING::VULKAN;

namespace
{
std::vector<uint32_t> textureVertShaderCode{
#include "texture.vert.inc"
};

std::vector<uint32_t> textureFragShaderCode{
#include "texture.frag.inc"
};

std::vector<uint32_t> textureFontsFragShaderCode{
#include "texture_fonts.frag.inc"
};

std::vector<uint32_t> textureNoBlendFragShaderCode{
#include "texture_noblend.frag.inc"
};

std::vector<uint32_t> textureMultiVertShaderCode{
#include "texture_multi.vert.inc"
};

std::vector<uint32_t> textureMultiFragShaderCode{
#include "texture_multi.frag.inc"
};

std::vector<uint32_t> textureMultiBlendColorFragShaderCode{
#include "texture_multi_blendcolor.frag.inc"
};

std::map<VULKANSHADER, std::pair<std::vector<uint32_t>, std::vector<uint32_t>>> shaderCode = {
    {VULKANSHADER::TEXTURE, {textureVertShaderCode, textureFragShaderCode}},
    {VULKANSHADER::FONTS, {textureVertShaderCode, textureFontsFragShaderCode}},
    {VULKANSHADER::TEXTURE_NOBLEND, {textureVertShaderCode, textureNoBlendFragShaderCode}},
    {VULKANSHADER::MULTI, {textureMultiVertShaderCode, textureMultiFragShaderCode}},
    {VULKANSHADER::MULTI_BLENDCOLOR,
     {textureMultiVertShaderCode, textureMultiBlendColorFragShaderCode}},
};

} // namespace

CVulkanShader::CVulkanShader(CVulkanDevice* device, VULKANSHADER method)
  : m_device(device), m_method(method)
{
}

bool CVulkanShader::InitializeShader(vk::Extent2D extent)
{
  if (!m_descriptorSetLayout)
    CreateUniqueDescriptorLayout();

  if (!m_pipelineLayout)
    CreateUniquePipelineLayout();

  if (!m_vertShaderModule)
    CreateUniqueVertexShaderModule();

  if (!m_fragShaderModule)
    CreateUniqueFragmentShaderModule();

  if (!m_pipeline)
    CreateUniquePipeline(extent);

  if (!m_sampler)
    CreateUniqueTextureSampler();

  return true;
}

void CVulkanShader::CreateUniqueDescriptorLayout()
{
  std::vector<vk::DescriptorSetLayoutBinding> descriptorSetLayoutBindings;

  vk::DescriptorSetLayoutBinding descriptorSetLayoutBinding;
  descriptorSetLayoutBinding.setBinding(0)
      .setDescriptorType(vk::DescriptorType::eUniformBuffer)
      .setDescriptorCount(1)
      .setStageFlags(vk::ShaderStageFlagBits::eVertex);

  descriptorSetLayoutBindings.emplace_back(descriptorSetLayoutBinding);

  descriptorSetLayoutBinding.setBinding(1)
      .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
      .setDescriptorCount(1)
      .setStageFlags(vk::ShaderStageFlagBits::eFragment);

  descriptorSetLayoutBindings.emplace_back(descriptorSetLayoutBinding);

  if (m_method == VULKANSHADER::MULTI || m_method == VULKANSHADER::MULTI_BLENDCOLOR)
  {
    descriptorSetLayoutBinding.setBinding(2)
        .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
        .setDescriptorCount(1)
        .setStageFlags(vk::ShaderStageFlagBits::eFragment);

    descriptorSetLayoutBindings.emplace_back(descriptorSetLayoutBinding);
  }

  vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;
  descriptorSetLayoutCreateInfo.setBindings(descriptorSetLayoutBindings);

  m_descriptorSetLayout =
      m_device->GetDevice().createDescriptorSetLayoutUnique(descriptorSetLayoutCreateInfo);
}

void CVulkanShader::CreateUniquePipelineLayout()
{
  vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
  pipelineLayoutCreateInfo.setSetLayouts(m_descriptorSetLayout.get());

  m_pipelineLayout = m_device->GetDevice().createPipelineLayoutUnique(pipelineLayoutCreateInfo);
}

void CVulkanShader::CreateUniqueVertexShaderModule()
{
  vk::ShaderModuleCreateInfo vertShaderModuleCreateInfo;
  vertShaderModuleCreateInfo.setCode(shaderCode[m_method].first);
  m_vertShaderModule = m_device->GetDevice().createShaderModuleUnique(vertShaderModuleCreateInfo);
}

void CVulkanShader::CreateUniqueFragmentShaderModule()
{
  vk::ShaderModuleCreateInfo fragShaderModuleCreateInfo;
  fragShaderModuleCreateInfo.setCode(shaderCode[m_method].second);
  m_fragShaderModule = m_device->GetDevice().createShaderModuleUnique(fragShaderModuleCreateInfo);
}

void CVulkanShader::CreateUniquePipeline(vk::Extent2D extent)
{
  vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
  vertShaderStageInfo.setStage(vk::ShaderStageFlagBits::eVertex)
      .setModule(m_vertShaderModule.get())
      .setPName("main");

  vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
  fragShaderStageInfo.setStage(vk::ShaderStageFlagBits::eFragment)
      .setModule(m_fragShaderModule.get())
      .setPName("main");

  std::vector<vk::PipelineShaderStageCreateInfo> pipelineShaderStages{vertShaderStageInfo,
                                                                      fragShaderStageInfo};

  auto bindingDescription = Vertex::GetBindingDescription();
  auto attributeDescriptions = Vertex::GetAttributeDescriptions();

  if (m_method == VULKANSHADER::MULTI || m_method == VULKANSHADER::MULTI_BLENDCOLOR)
    attributeDescriptions = Vertex::GetMultiAttributeDescriptions();

  vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
  vertexInputInfo.setVertexBindingDescriptions(bindingDescription)
      .setVertexAttributeDescriptions(attributeDescriptions);

  vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
  inputAssembly.setTopology(vk::PrimitiveTopology::eTriangleList).setPrimitiveRestartEnable(false);

  vk::Viewport viewport;
  viewport.setX(0.0f)
      .setY(0.0f)
      .setWidth(static_cast<float>(extent.width))
      .setHeight(static_cast<float>(extent.height))
      .setMinDepth(0.0f)
      .setMaxDepth(1.0f);

  vk::Rect2D scissor;
  scissor.setOffset({0, 0}).setExtent(extent);

  vk::PipelineViewportStateCreateInfo viewportState;
  viewportState.setViewports(viewport).setScissors(scissor);

  vk::PipelineRasterizationStateCreateInfo rasterizer;
  rasterizer.setDepthClampEnable(false)
      .setRasterizerDiscardEnable(false)
      .setPolygonMode(vk::PolygonMode::eFill)
      .setCullMode({})
      .setFrontFace(vk::FrontFace::eCounterClockwise)
      .setLineWidth(1.0f);

  vk::PipelineMultisampleStateCreateInfo multisampling;
  multisampling.setRasterizationSamples(vk::SampleCountFlagBits::e1)
      .setSampleShadingEnable(false)
      .setMinSampleShading(1.0);

  vk::PipelineColorBlendAttachmentState colorBlendAttachmentAlpha;
  colorBlendAttachmentAlpha.setBlendEnable(true)
      .setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
      .setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
      .setColorBlendOp(vk::BlendOp::eAdd)
      .setSrcAlphaBlendFactor(vk::BlendFactor::eOneMinusDstAlpha)
      .setDstAlphaBlendFactor(vk::BlendFactor::eOne)
      .setAlphaBlendOp(vk::BlendOp::eAdd)
      .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                         vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

  vk::PipelineColorBlendAttachmentState colorBlendAttachment;
  colorBlendAttachment.setBlendEnable(false)
      .setSrcColorBlendFactor({})
      .setDstColorBlendFactor({})
      .setColorBlendOp({})
      .setSrcAlphaBlendFactor({})
      .setDstAlphaBlendFactor({})
      .setAlphaBlendOp({})
      .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                         vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

  vk::PipelineColorBlendStateCreateInfo colorBlending;
  colorBlending.setLogicOpEnable(false)
      .setLogicOp(vk::LogicOp::eCopy)
      .setAttachments(colorBlendAttachment);

  vk::GraphicsPipelineCreateInfo pipelineCreateInfo;
  pipelineCreateInfo.setStages(pipelineShaderStages)
      .setPVertexInputState(&vertexInputInfo)
      .setPInputAssemblyState(&inputAssembly)
      .setPViewportState(&viewportState)
      .setPRasterizationState(&rasterizer)
      .setPMultisampleState(&multisampling)
      .setPColorBlendState(&colorBlending)
      .setLayout(m_pipelineLayout.get())
      .setRenderPass(m_device->GetRenderPass())
      .setSubpass(0);


  auto result = m_device->GetDevice().createGraphicsPipelineUnique({}, pipelineCreateInfo);

  if (result.result != vk::Result::eSuccess)
    throw std::runtime_error("something went wrong!");

  m_pipeline = std::move(result.value);

  colorBlending.setAttachments(colorBlendAttachmentAlpha);

  result = m_device->GetDevice().createGraphicsPipelineUnique({}, pipelineCreateInfo);

  if (result.result != vk::Result::eSuccess)
    throw std::runtime_error("something went wrong!");

  m_pipelineAlpha = std::move(result.value);
}

void CVulkanShader::CreateUniqueTextureSampler()
{
  vk::SamplerCreateInfo samplerCreateInfo;
  samplerCreateInfo.setMagFilter(vk::Filter::eLinear)
      .setMinFilter(vk::Filter::eLinear)
      .setMipmapMode(vk::SamplerMipmapMode::eNearest)
      .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
      .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
      .setAddressModeW(vk::SamplerAddressMode::eRepeat)
      .setAnisotropyEnable(false)
      .setCompareEnable(false)
      .setCompareOp(vk::CompareOp::eAlways)
      .setBorderColor(vk::BorderColor::eIntOpaqueBlack)
      .setUnnormalizedCoordinates(false);

  m_sampler = m_device->GetDevice().createSamplerUnique(samplerCreateInfo);
}

vk::UniqueDescriptorPool CVulkanShader::CreateUniqueDescriptorPool()
{
  std::vector<vk::DescriptorPoolSize> descriptorPoolSize = {
      vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer,
                             m_device->GetSwapchainImagesSize()),
      vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler,
                             m_device->GetSwapchainImagesSize())};

  if (m_method == VULKANSHADER::MULTI || m_method == VULKANSHADER::MULTI_BLENDCOLOR)
  {
    descriptorPoolSize.emplace_back(vk::DescriptorPoolSize(
        vk::DescriptorType::eCombinedImageSampler, m_device->GetSwapchainImagesSize()));
  }

  vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo;
  descriptorPoolCreateInfo.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
      .setMaxSets(m_device->GetSwapchainImagesSize())
      .setPoolSizes(descriptorPoolSize);

  return m_device->GetDevice().createDescriptorPoolUnique(descriptorPoolCreateInfo);
}

std::vector<vk::UniqueDescriptorSet> CVulkanShader::CreateUniqueDescriptorSets(
    vk::UniqueDescriptorPool& descriptorPool)
{
  std::vector<vk::DescriptorSetLayout> descriptorSetLayouts(m_device->GetSwapchainImagesSize(),
                                                            m_descriptorSetLayout.get());

  vk::DescriptorSetAllocateInfo descriptorSetAllocateInfo;
  descriptorSetAllocateInfo.setDescriptorPool(descriptorPool.get())
      .setSetLayouts(descriptorSetLayouts);

  return m_device->GetDevice().allocateDescriptorSetsUnique(descriptorSetAllocateInfo);
}

void CVulkanShader::UpdateDescriptorWrites(std::vector<vk::UniqueDescriptorSet>& descriptorSets,
                                           vk::ImageView& imageView)
{
  for (size_t i = 0; i < m_device->GetSwapchainImagesSize(); i++)
  {
    vk::DescriptorBufferInfo descriptorBufferInfo;
    descriptorBufferInfo.setBuffer(m_device->GetUniformBuffer(i))
        .setOffset(0)
        .setRange(sizeof(UniformBufferObject));

    vk::DescriptorImageInfo descriptorImageInfo;
    descriptorImageInfo.setSampler(m_sampler.get())
        .setImageView(imageView)
        .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

    std::vector<vk::WriteDescriptorSet> descriptorWrites;

    vk::WriteDescriptorSet writeDescriptorSet;
    writeDescriptorSet.setDstSet(descriptorSets[i].get())
        .setDstBinding(0)
        .setDstArrayElement(0)
        .setDescriptorType(vk::DescriptorType::eUniformBuffer)
        .setBufferInfo(descriptorBufferInfo);

    descriptorWrites.emplace_back(writeDescriptorSet);

    writeDescriptorSet.setDstSet(descriptorSets[i].get())
        .setDstBinding(1)
        .setDstArrayElement(0)
        .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
        .setImageInfo(descriptorImageInfo);

    descriptorWrites.emplace_back(writeDescriptorSet);

    if (m_method == VULKANSHADER::MULTI || m_method == VULKANSHADER::MULTI_BLENDCOLOR)
    {
      writeDescriptorSet.setDstSet(descriptorSets[i].get())
          .setDstBinding(2)
          .setDstArrayElement(0)
          .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
          .setImageInfo(descriptorImageInfo);

      descriptorWrites.emplace_back(writeDescriptorSet);
    }

    m_device->GetDevice().updateDescriptorSets(descriptorWrites, {});
  }
}

void CVulkanShader::UpdatePipeline(vk::Extent2D extent)
{
  CreateUniquePipeline(extent);
}

void CVulkanShader::UpdateCommandBuffers(std::vector<vk::UniqueCommandBuffer>& commandBuffers,
                                         std::vector<vk::UniqueDescriptorSet>& descriptorSets,
                                         vk::Buffer& vertexBuffer,
                                         vk::Buffer& indexBuffer)
{
  int i = m_device->GetImageIndex();

  vk::CommandBufferBeginInfo commandBufferBeginInfo(
      vk::CommandBufferUsageFlagBits::eSimultaneousUse);

  vk::Result result = commandBuffers[i]->begin(&commandBufferBeginInfo);

  if (result != vk::Result::eSuccess)
    throw std::runtime_error("something went wrong!");

  vk::ClearValue clearValue = {};
  vk::RenderPassBeginInfo renderPassBeginInfo(m_device->GetRenderPass(),
                                              m_device->GetFramebuffer(i), {}, 1, &clearValue);

  commandBuffers[i]->beginRenderPass(&renderPassBeginInfo, vk::SubpassContents::eInline);

  if (m_alpha)
    commandBuffers[i]->bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipelineAlpha.get());
  else
    commandBuffers[i]->bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.get());

  commandBuffers[i]->bindVertexBuffers(0, vertexBuffer, vk::DeviceSize(0));
  commandBuffers[i]->bindIndexBuffer(indexBuffer, 0, vk::IndexType::eUint16);

  commandBuffers[i]->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout.get(), 0,
                                        descriptorSets[i].get(), {});

  commandBuffers[i]->drawIndexed(static_cast<uint32_t>(6), 1, 0, 0, 0);

  commandBuffers[i]->endRenderPass();
  commandBuffers[i]->end();
}
