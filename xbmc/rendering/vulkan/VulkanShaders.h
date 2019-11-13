/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

namespace KODI
{
namespace RENDERING
{
namespace VULKAN
{

enum class VULKANSHADER
{
  // DEFAULT,
  TEXTURE,
  MULTI,
  FONTS,
  TEXTURE_NOBLEND,
  MULTI_BLENDCOLOR,
  // TEXTURE_RGBA,
  // TEXTURE_RGBA_OES,
  // TEXTURE_RGBA_BLENDCOLOR,
  // TEXTURE_RGBA_BOB,
  // TEXTURE_RGBA_BOB_OES,
  // TEXTURE_NOALPHA,
  MAX
};
}
} // namespace RENDERING
} // namespace KODI
