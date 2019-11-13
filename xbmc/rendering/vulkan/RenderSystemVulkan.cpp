/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "RenderSystemVulkan.h"

#include "CompileInfo.h"
#include "VulkanShader.h"
#include "guilib/GUITextureVulkan.h"
#include "utils/log.h"

using namespace KODI::RENDERING::VULKAN;

namespace
{
std::array<const char*, 1> layerNames =
{
  "VK_LAYER_KHRONOS_validation",
};

} // namespace

CRenderSystemVulkan::CRenderSystemVulkan()
{
  m_instanceExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
}

bool CRenderSystemVulkan::InitRenderSystem()
{
  if (m_initialized)
    return true;

  try
  {
    vk::ApplicationInfo appInfo(CCompileInfo::GetAppName(), 0, nullptr, 0, VK_API_VERSION_1_0);

    vk::InstanceCreateInfo instanceCreateInfo;
    instanceCreateInfo.setPApplicationInfo(&appInfo)
        .setPEnabledLayerNames(layerNames)
        .setPEnabledExtensionNames(m_instanceExtensions);

    m_instance = vk::createInstanceUnique(instanceCreateInfo);

    m_debug = std::make_unique<CVulkanDebug>(m_instance);
  }
  catch (vk::SystemError& err)
  {
    CLog::LogF(LOGERROR, "vk::SystemError: {}", err.what());
    return false;
  }
  catch (std::runtime_error& err)
  {
    CLog::LogF(LOGERROR, "std::runtime_error: {}", err.what());
    return false;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "unknown error");
    return false;
  }

  CGUITextureVulkan::Register();

  m_initialized = true;

  return true;
}

bool CRenderSystemVulkan::DestroyRenderSystem()
{
  for (auto& shader : m_shaders)
    shader.reset();

  try
  {
    m_device.reset();
    m_nativeSurface.reset();
  }
  catch (vk::SystemError& err)
  {
    CLog::LogF(LOGERROR, "vk::SystemError: {}", err.what());
    return false;
  }
  catch (std::runtime_error& err)
  {
    CLog::LogF(LOGERROR, "std::runtime_error: {}", err.what());
    return false;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "unknown error");
    return false;
  }

  return true;
}

bool CRenderSystemVulkan::ResetRenderSystem(int width, int height)
{
  m_width = width;
  m_height = height;

  auto extent = vk::Extent2D(width, height);

  try
  {
    if (!m_device)
    {
      m_device = std::make_unique<CVulkanDevice>();
      m_device->CreatePhysicalDevice(m_instance.get());
      m_device->CreateLogicalDevice(m_nativeSurface.get());
    }

    m_device->WaitIdle();

    m_device->CreateSwapChain(m_nativeSurface.get(), extent);
    m_device->CreateRenderPass();
    // m_device->CreateDescriptorLayout();
    // m_device->CreatePipeline();
    m_device->CreateFrameBuffer();
    m_device->CreateCommandPool();
    // m_device->CreateTextureImage();
    // m_device->CreateTextureImageView();
    // m_device->CreateTextureSampler();
    // m_device->CreateVertexBuffer();
    // m_device->CreateIndexBuffer();
    m_device->CreateUniformBuffers();
    // m_device->CreateDescriptorPool();
    // m_device->CreateDescriptorSets();
    // m_device->CreateCommandBuffer();
    m_device->CreateSyncObjects();

    if (!m_shaders[static_cast<int>(VULKANSHADER::TEXTURE)])
    {
      m_shaders[static_cast<int>(VULKANSHADER::TEXTURE)] =
          std::make_unique<CVulkanShader>(m_device.get(), VULKANSHADER::TEXTURE);
      m_shaders[static_cast<int>(VULKANSHADER::TEXTURE)]->InitializeShader(extent);
    }
    else
    {
      m_shaders[static_cast<int>(VULKANSHADER::TEXTURE)]->UpdatePipeline(extent);
    }

    if (!m_shaders[static_cast<int>(VULKANSHADER::FONTS)])
    {
      m_shaders[static_cast<int>(VULKANSHADER::FONTS)] =
          std::make_unique<CVulkanShader>(m_device.get(), VULKANSHADER::FONTS);
      m_shaders[static_cast<int>(VULKANSHADER::FONTS)]->InitializeShader(extent);
    }
    else
    {
      m_shaders[static_cast<int>(VULKANSHADER::FONTS)]->UpdatePipeline(extent);
    }

    if (!m_shaders[static_cast<int>(VULKANSHADER::TEXTURE_NOBLEND)])
    {
      m_shaders[static_cast<int>(VULKANSHADER::TEXTURE_NOBLEND)] =
          std::make_unique<CVulkanShader>(m_device.get(), VULKANSHADER::TEXTURE_NOBLEND);
      m_shaders[static_cast<int>(VULKANSHADER::TEXTURE_NOBLEND)]->InitializeShader(extent);
    }
    else
    {
      m_shaders[static_cast<int>(VULKANSHADER::MULTI)]->UpdatePipeline(extent);
    }

    if (!m_shaders[static_cast<int>(VULKANSHADER::MULTI)])
    {
      m_shaders[static_cast<int>(VULKANSHADER::MULTI)] =
          std::make_unique<CVulkanShader>(m_device.get(), VULKANSHADER::MULTI);
      m_shaders[static_cast<int>(VULKANSHADER::MULTI)]->InitializeShader(extent);
    }
    else
    {
      m_shaders[static_cast<int>(VULKANSHADER::MULTI)]->UpdatePipeline(extent);
    }

    if (!m_shaders[static_cast<int>(VULKANSHADER::MULTI_BLENDCOLOR)])
    {
      m_shaders[static_cast<int>(VULKANSHADER::MULTI_BLENDCOLOR)] =
          std::make_unique<CVulkanShader>(m_device.get(), VULKANSHADER::MULTI_BLENDCOLOR);
      m_shaders[static_cast<int>(VULKANSHADER::MULTI_BLENDCOLOR)]->InitializeShader(extent);
    }
    else
    {
      m_shaders[static_cast<int>(VULKANSHADER::MULTI_BLENDCOLOR)]->UpdatePipeline(extent);
    }
  }
  catch (vk::SystemError& err)
  {
    CLog::LogF(LOGERROR, "vk::SystemError: {}", err.what());
    return false;
  }
  catch (std::runtime_error& err)
  {
    CLog::LogF(LOGERROR, "std::runtime_error: {}", err.what());
    return false;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "unknown error");
    return false;
  }

  return true;
}

bool CRenderSystemVulkan::BeginRender()
{
  m_device->Begin();

  return true;
}

bool CRenderSystemVulkan::EndRender()
{
  m_device->End();

  return true;
}

bool CRenderSystemVulkan::Present()
{
  m_device->Present();

  return true;
}

bool CRenderSystemVulkan::ClearBuffers(UTILS::COLOR::Color color)
{
  return false;
}

bool CRenderSystemVulkan::IsExtSupported(const char* extension) const
{
  return false;
}

void CRenderSystemVulkan::SetViewPort(const CRect& viewPort)
{
}

void CRenderSystemVulkan::GetViewPort(CRect& viewPort)
{
}

void CRenderSystemVulkan::SetScissors(const CRect& rect)
{
}

void CRenderSystemVulkan::ResetScissors()
{
}

void CRenderSystemVulkan::CaptureStateBlock()
{
}

void CRenderSystemVulkan::ApplyStateBlock()
{
}

void CRenderSystemVulkan::SetCameraPosition(const CPoint& camera, int screenWidth, int screenHeight, float stereoFactor)
{
}

void CRenderSystemVulkan::DestroySwapChain()
{
  m_device->DestroySwapChain();
}

CVulkanShader* CRenderSystemVulkan::GetShader(VULKANSHADER method)
{
  if (m_shaders[static_cast<int>(method)])
    return m_shaders[static_cast<int>(method)].get();

  return nullptr;
}
