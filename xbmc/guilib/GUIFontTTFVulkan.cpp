/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GUIFontTTFVulkan.h"

#include "GUIFont.h"
#include "ServiceBroker.h"
#include "rendering/vulkan/VulkanShaders.h"
#include "utils/log.h"

#include <ft2build.h>

#include FT_FREETYPE_H
#include FT_GLYPH_H

CGUIFontTTF* CGUIFontTTF::CreateGUIFontTTF(const std::string& fileName)
{
  return new CGUIFontTTFVulkan(fileName);
}

CGUIFontTTFVulkan::CGUIFontTTFVulkan(const std::string& strFileName) : CGUIFontTTF(strFileName)
{
  m_renderSystem = dynamic_cast<KODI::RENDERING::VULKAN::CRenderSystemVulkan*>(
      CServiceBroker::GetRenderSystem());

  m_device = m_renderSystem->GetDevice();
}

CGUIFontTTFVulkan::~CGUIFontTTFVulkan()
{
  m_device->WaitIdle();
}

bool CGUIFontTTFVulkan::FirstBegin()
{
  CLog::Log(LOGDEBUG, "CGUIFontTTFVulkan::{}", __FUNCTION__);

  if (!m_indexBuffer && !m_indexBufferMemory)
    std::tie(m_indexBuffer, m_indexBufferMemory) = m_device->CreateUniqueIndexBuffer();

  auto shader = m_renderSystem->GetShader(KODI::RENDERING::VULKAN::VULKANSHADER::FONTS);

  shader->SetAlpha(true);

  if (!m_descriptorPool)
    m_descriptorPool = shader->CreateUniqueDescriptorPool();

  if (m_descriptorSets.empty())
  {
    m_descriptorSets = shader->CreateUniqueDescriptorSets(m_descriptorPool);
    shader->UpdateDescriptorWrites(m_descriptorSets, m_imageView.get());
  }

  if (m_commandBuffers.empty())
    m_commandBuffers = m_device->CreateUniqueCommandBuffers();

  return true;
}

void CGUIFontTTFVulkan::LastEnd()
{
  CLog::Log(LOGDEBUG, "CGUIFontTTFVulkan::{} - m_vertex:{}", __FUNCTION__, m_vertex.size());

  if (m_vertex.size() == 0)
    return;

  //! @todo: finish

  // if (m_vertex.size() != m_vertices.size())
  // {
  //   std::tie(m_vertexBuffer, m_vertexBufferMemory) = m_device->CreateUniqueVertexBuffer(m_vertex.size());

  //   m_vertices.clear();
  //   for (const auto& v : m_vertex)
  //   {
  //     KODI::RENDERING::VULKAN::Vertex vertex;
  //     vertex.pos = glm::vec3(v.x, v.y, v.z);
  //     vertex.color = glm::vec4(v.r, v.g, v.b, v.a);
  //     vertex.texCoord0 = glm::vec2(v.u, v.v);
  //     m_vertices.emplace_back(vertex);
  //   }
  // }

  // CLog::Log(LOGDEBUG, "CGUIFontTTFVulkan::{} - m_vertices:{}", __FUNCTION__, m_vertices.size());

  // m_device->UpdateVertexBuffer(m_vertexBufferMemory.get(), m_vertices);
  // m_device->UpdateIndexBuffer(m_indexBufferMemory.get(), m_indices);

  // auto shader = m_renderSystem->GetShader(KODI::RENDERING::VULKAN::VULKANSHADER::FONTS);

  // shader->UpdateCommandBuffers(m_commandBuffers, m_descriptorSets, m_vertexBuffer.get(),
  //                              m_indexBuffer.get());

  // m_device->AddCommandBuffers(m_commandBuffers);
}

void CGUIFontTTFVulkan::AddVertex(CGUIFontTTFVulkan* font, KODI::RENDERING::VULKAN::Vertex& vertex)
{
  font->m_vertices.emplace_back(vertex);
}

CVertexBuffer CGUIFontTTFVulkan::CreateVertexBuffer(const std::vector<SVertex>& vertices) const
{
  for (const auto& v : vertices)
  {
    KODI::RENDERING::VULKAN::Vertex vertex;
    vertex.pos = glm::vec3(v.x, v.y, v.z);
    vertex.color = glm::vec4(v.r, v.g, v.b, v.a);
    vertex.texCoord0 = glm::vec2(v.u, v.v);
    AddVertex(const_cast<CGUIFontTTFVulkan*>(this), vertex);
  }

  CLog::Log(LOGDEBUG, "CGUIFontTTFVulkan::{} - m_vertices:{}", __FUNCTION__, m_vertices.size());

  return CVertexBuffer(reinterpret_cast<void*>(
                           const_cast<std::vector<KODI::RENDERING::VULKAN::Vertex>*>(&m_vertices)),
                       vertices.size() / 4, this);
}

void CGUIFontTTFVulkan::DestroyVertexBuffer(CVertexBuffer& bufferHandle) const
{
}

std::unique_ptr<CTexture> CGUIFontTTFVulkan::ReallocTexture(unsigned int& newHeight)
{
  auto extent2D = vk::Extent2D(m_textureWidth, newHeight);

  CLog::Log(LOGDEBUG, "CGUIFontTTFVulkan::{} - width:{} height:{}", __FUNCTION__, m_textureWidth,
            newHeight);

  vk::UniqueImage image;
  vk::UniqueDeviceMemory deviceMemory;

  std::tie(image, deviceMemory) = m_device->CreateUniqueTextureImage(extent2D);

  m_image = std::move(image);
  m_deviceMemory = std::move(deviceMemory);

  m_imageView = m_device->CreateUniqueTextureImageView(m_image.get());

  m_textureHeight = newHeight;

  return std::make_unique<CTextureVulkan>(m_textureWidth, m_textureHeight, XB_FMT_A8);
}

bool CGUIFontTTFVulkan::CopyCharToTexture(
    FT_BitmapGlyph bitGlyph, unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2)
{
  if (!m_image && !m_device)
    return false;

  CLog::Log(LOGDEBUG, "CGUIFontTTFVulkan::{} - x1:{} x2:{} y1:{} y2:{}", __FUNCTION__, x1, x2, y1,
            y2);

  auto extent3D = vk::Extent3D(x2, y2, 1);
  auto offset3D = vk::Offset3D(x1, y1, 0);

  if (extent3D.width == 0 || extent3D.height == 0)
    return false;

  auto bitmap = bitGlyph->bitmap;

  m_device->CopyRegion(m_image.get(), vk::Extent3D(m_textureWidth, m_textureHeight, 1), extent3D,
                       offset3D, vk::Format::eR8Snorm, bitmap.buffer, bitmap.pitch);

  return true;
}

void CGUIFontTTFVulkan::DeleteHardwareTexture()
{
}
