/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "TextureVulkan.h"

#include "ServiceBroker.h"

std::unique_ptr<CTexture> CTexture::CreateTexture(unsigned int width,
                                                  unsigned int height,
                                                  unsigned int format)
{
  return std::make_unique<CTextureVulkan>(width, height, format);
}

CTextureVulkan::CTextureVulkan(unsigned int width, unsigned int height, unsigned int format)
  : CTexture(width, height, format)
{
  m_renderSystem = dynamic_cast<KODI::RENDERING::VULKAN::CRenderSystemVulkan*>(
      CServiceBroker::GetRenderSystem());

  switch (format)
  {
    case XB_FMT_A8:
      m_vkFormat = vk::Format::eR8Snorm;
      break;
    case XB_FMT_RGB8:
      m_vkFormat = vk::Format::eR8G8B8Snorm;
      break;
    case XB_FMT_A8R8G8B8:
    case XB_FMT_RGBA8:
    default:
      m_vkFormat = vk::Format::eR8G8B8A8Snorm;
      break;
  }

  m_device = m_renderSystem->GetDevice();
}

CTextureVulkan::~CTextureVulkan()
{
  m_device->WaitIdle();
}

void CTextureVulkan::LoadToGPU()
{
  if (!m_image && !m_deviceMemory)
  {
    auto extent2D = vk::Extent2D(m_textureWidth, m_textureHeight);

    std::tie(m_image, m_deviceMemory) = m_device->CreateUniqueTextureImage(extent2D);

    auto extent3D = vk::Extent3D(extent2D, 1);
    auto offset3D = vk::Offset3D(0, 0, 0);

    m_device->CopyRegion(m_image.get(), extent3D, extent3D, offset3D, m_vkFormat, m_pixels,
                         m_textureWidth);
  }

  if (!m_imageView)
    m_imageView = m_device->CreateUniqueTextureImageView(m_image.get());
}
