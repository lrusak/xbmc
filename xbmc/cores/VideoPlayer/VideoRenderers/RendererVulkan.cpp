/*
 *  Copyright (C) 2010-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "RendererVulkan.h"

#include "RenderFactory.h"

CBaseRenderer* CRendererVulkan::Create(CVideoBuffer* buffer)
{
  return new CRendererVulkan();
}

bool CRendererVulkan::Register()
{
  VIDEOPLAYER::CRendererFactory::RegisterRenderer("vulkan", CRendererVulkan::Create);
  return true;
}
