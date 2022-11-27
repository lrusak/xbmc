/*
 *  Copyright (C) 2017-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "RenderBufferPoolDMA.h"

#include "RenderBufferDMA.h"
#include "ServiceBroker.h"
#include "cores/RetroPlayer/rendering/VideoRenderers/RPRendererDMA.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "settings/lib/Setting.h"

#include <drm_fourcc.h>

using namespace KODI;
using namespace RETRO;

namespace
{

constexpr auto SETTING_RETROPLAYER_USEDMARENDERER = "retroplayer.usedmarenderer";

}

CRenderBufferPoolDMA::CRenderBufferPoolDMA(CRenderContext& context) : m_context(context)
{
}

bool CRenderBufferPoolDMA::IsCompatible(const CRenderVideoSettings& renderSettings) const
{
  if (!CRPRendererDMA::SupportsScalingMethod(renderSettings.GetScalingMethod()))
    return false;

  const auto settingsComponent = CServiceBroker::GetSettingsComponent();
  if (!settingsComponent)
    return false;

  const auto settings = settingsComponent->GetSettings();
  if (!settings)
    return false;

  return settings->GetBool(SETTING_RETROPLAYER_USEDMARENDERER);
}

IRenderBuffer* CRenderBufferPoolDMA::CreateRenderBuffer(void* header /* = nullptr */)
{
  return new CRenderBufferDMA(m_context, m_fourcc);
}

bool CRenderBufferPoolDMA::ConfigureInternal()
{
  switch (m_format)
  {
    case AV_PIX_FMT_0RGB32:
    {
      m_fourcc = DRM_FORMAT_ARGB8888;
      return true;
    }
    case AV_PIX_FMT_RGB555:
    {
      m_fourcc = DRM_FORMAT_ARGB1555;
      return true;
    }
    case AV_PIX_FMT_RGB565:
    {
      m_fourcc = DRM_FORMAT_RGB565;
      return true;
    }
    default:
      break; // we shouldn't even get this far if we are given an unsupported pixel format
  }

  return false;
}
