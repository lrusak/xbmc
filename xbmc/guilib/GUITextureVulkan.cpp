/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GUITextureVulkan.h"

#include "ServiceBroker.h"
#include "TextureVulkan.h"
#include "rendering/vulkan/VulkanShader.h"
#include "utils/log.h"

namespace
{

glm::vec4 GetChannels(uint32_t color)
{
  return glm::vec4(static_cast<float>((color >> 16) & 0xFF) / 255.0f,
                   static_cast<float>((color >> 8) & 0xFF) / 255.0f,
                   static_cast<float>((color >> 0) & 0xFF) / 255.0f,
                   static_cast<float>((color >> 24) & 0xFF) / 255.0f);
}

} // namespace

void CGUITextureVulkan::Register()
{
  CGUITexture::Register(CGUITextureVulkan::CreateTexture, CGUITextureVulkan::DrawQuad);
}

CGUITexture* CGUITextureVulkan::CreateTexture(
    float posX, float posY, float width, float height, const CTextureInfo& texture)
{
  return new CGUITextureVulkan(posX, posY, width, height, texture);
}

CGUITextureVulkan::CGUITextureVulkan(
    float posX, float posY, float width, float height, const CTextureInfo& texture)
  : CGUITexture(posX, posY, width, height, texture)
{
  m_renderSystem = dynamic_cast<KODI::RENDERING::VULKAN::CRenderSystemVulkan*>(
      CServiceBroker::GetRenderSystem());

  m_device = m_renderSystem->GetDevice();

  m_vertices.resize(4);
}

CGUITextureVulkan::CGUITextureVulkan(const CGUITextureVulkan& texture)
  : CGUITexture(texture),
    m_renderSystem(texture.m_renderSystem),
    m_device(texture.m_device),
    m_vertices(texture.m_vertices)
{
}

CGUITextureVulkan::~CGUITextureVulkan()
{
  m_device->WaitIdle();
}

CGUITextureVulkan* CGUITextureVulkan::Clone() const
{
  return new CGUITextureVulkan(*this);
}

void CGUITextureVulkan::Begin(UTILS::COLOR::Color color)
{
  // CLog::Log(LOGDEBUG, "CGUITextureVulkan::{}", __FUNCTION__);

  auto texture = static_cast<CTextureVulkan*>(m_texture.m_textures[m_currentFrame].get());
  texture->LoadToGPU();

  if (m_diffuse.size())
    m_diffuse.m_textures[0]->LoadToGPU();

  if (!m_vertexBuffer && !m_vertexBufferMemory)
    std::tie(m_vertexBuffer, m_vertexBufferMemory) =
        m_device->CreateUniqueVertexBuffer(m_vertices.size());

  if (!m_indexBuffer && !m_indexBufferMemory)
    std::tie(m_indexBuffer, m_indexBufferMemory) = m_device->CreateUniqueIndexBuffer();

  auto channels = GetChannels(color);
  for (size_t i = 0; i < m_vertices.size(); i++)
    m_vertices[i].color = channels;

  // CLog::Log(LOGDEBUG, "CGUITextureVulkan::{} - R:{} B:{} G:{} A:{}", __FUNCTION__, channels.r, channels.g, channels.b, channels.a);

  bool hasAlpha = m_texture.m_textures[m_currentFrame]->HasAlpha() || channels.a < 1.0f;

  if (m_diffuse.size() > 0)
  {
    if (channels.r == 1.0f && channels.g == 1.0f && channels.b == 1.0f && channels.a == 1.0f)
      m_shaderMethod = KODI::RENDERING::VULKAN::VULKANSHADER::MULTI;
    else
      m_shaderMethod = KODI::RENDERING::VULKAN::VULKANSHADER::MULTI_BLENDCOLOR;

    hasAlpha |= m_diffuse.m_textures[0]->HasAlpha();
  }
  else
  {
    if (channels.r == 1.0f && channels.g == 1.0f && channels.b == 1.0f && channels.a == 1.0f)
      m_shaderMethod = KODI::RENDERING::VULKAN::VULKANSHADER::TEXTURE_NOBLEND;
    else
      m_shaderMethod = KODI::RENDERING::VULKAN::VULKANSHADER::TEXTURE;
  }

  // CLog::Log(LOGDEBUG, "CGUITextureVulkan::{} - shader:{}", __FUNCTION__, static_cast<int>(m_shaderMethod));
  // CLog::Log(LOGDEBUG, "CGUITextureVulkan::{} - alpha:{}", __FUNCTION__, hasAlpha);

  auto shader = m_renderSystem->GetShader(m_shaderMethod);

  shader->SetAlpha(hasAlpha);

  if (!m_descriptorPool)
    m_descriptorPool = shader->CreateUniqueDescriptorPool();

  if (m_descriptorSets.empty())
  {
    m_descriptorSets = shader->CreateUniqueDescriptorSets(m_descriptorPool);
    shader->UpdateDescriptorWrites(m_descriptorSets, texture->GetImageView());
  }

  if (m_commandBuffers.empty())
    m_commandBuffers = m_device->CreateUniqueCommandBuffers();
}

void CGUITextureVulkan::End()
{
  // CLog::Log(LOGDEBUG, "CGUITextureVulkan::{}", __FUNCTION__);

  m_device->UpdateVertexBuffer(m_vertexBufferMemory.get(), m_vertices);
  m_device->UpdateIndexBuffer(m_indexBufferMemory.get(), m_indices);

  auto shader = m_renderSystem->GetShader(m_shaderMethod);

  shader->UpdateCommandBuffers(m_commandBuffers, m_descriptorSets, m_vertexBuffer.get(),
                               m_indexBuffer.get());

  m_device->AddCommandBuffers(m_commandBuffers);
}

void CGUITextureVulkan::Draw(
    float* x, float* y, float* z, const CRect& texture, const CRect& diffuse, int orientation)
{
  // CLog::Log(LOGDEBUG, "CGUITextureVulkan::{}", __FUNCTION__);

  m_vertices[0].pos = glm::vec3(x[0], y[0], z[0]);
  m_vertices[1].pos = glm::vec3(x[1], y[1], z[1]);
  m_vertices[2].pos = glm::vec3(x[2], y[2], z[2]);
  m_vertices[3].pos = glm::vec3(x[3], y[3], z[3]);

  m_vertices[0].texCoord0 = glm::vec2(texture.x1, texture.y1);
  m_vertices[1].texCoord0 = glm::vec2(texture.x2, texture.y1);
  m_vertices[2].texCoord0 = glm::vec2(texture.x2, texture.y2);
  m_vertices[3].texCoord0 = glm::vec2(texture.x1, texture.y2);

  if (m_diffuse.size() > 0)
  {
    m_vertices[0].texCoord1 = glm::vec2(diffuse.x1, diffuse.y1);
    m_vertices[1].texCoord1 = glm::vec2(diffuse.x2, diffuse.y1);
    m_vertices[2].texCoord1 = glm::vec2(diffuse.x2, diffuse.y2);
    m_vertices[3].texCoord1 = glm::vec2(diffuse.x1, diffuse.y2);
  }
}

void CGUITextureVulkan::DrawQuad(const CRect& rect,
                                 UTILS::COLOR::Color color,
                                 CTexture* texture,
                                 const CRect* texCoords)
{
}
