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

struct UniformBufferObject
{
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 proj;
};

} // namespace VULKAN
} // namespace RENDERING
} // namespace KODI
