/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

namespace KODI
{
namespace RENDERING
{
namespace VULKAN
{

struct Vertex
{
  glm::vec3 pos;
  glm::vec4 color;
  glm::vec2 texCoord0;
  glm::vec2 texCoord1;

  static vk::VertexInputBindingDescription GetBindingDescription()
  {
    vk::VertexInputBindingDescription bindingDescription(0, sizeof(Vertex),
                                                         vk::VertexInputRate::eVertex);

    return bindingDescription;
  }

  static std::vector<vk::VertexInputAttributeDescription> GetAttributeDescriptions()
  {
    std::vector<vk::VertexInputAttributeDescription> attributeDescriptions(3);

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
    attributeDescriptions[0].offset = offsetof(Vertex, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = vk::Format::eR32G32B32A32Sfloat;
    attributeDescriptions[1].offset = offsetof(Vertex, color);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = vk::Format::eR32G32Sfloat;
    attributeDescriptions[2].offset = offsetof(Vertex, texCoord0);

    return attributeDescriptions;
  }

  static std::vector<vk::VertexInputAttributeDescription> GetMultiAttributeDescriptions()
  {
    std::vector<vk::VertexInputAttributeDescription> attributeDescriptions(4);

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
    attributeDescriptions[0].offset = offsetof(Vertex, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = vk::Format::eR32G32B32A32Sfloat;
    attributeDescriptions[1].offset = offsetof(Vertex, color);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = vk::Format::eR32G32Sfloat;
    attributeDescriptions[2].offset = offsetof(Vertex, texCoord0);

    attributeDescriptions[3].binding = 0;
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format = vk::Format::eR32G32Sfloat;
    attributeDescriptions[3].offset = offsetof(Vertex, texCoord1);

    return attributeDescriptions;
  }
};

} // namespace VULKAN
} // namespace RENDERING
} // namespace KODI
