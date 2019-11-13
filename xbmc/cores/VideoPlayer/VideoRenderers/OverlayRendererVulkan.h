/*
 *      Initial code sponsored by: Voddler Inc (voddler.com)
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "OverlayRenderer.h"

class CDVDOverlayImage;
class CDVDOverlaySpu;

namespace OVERLAY
{

class COverlayTextureVulkan : public COverlay
{
public:
  explicit COverlayTextureVulkan(CDVDOverlayImage* o);
  explicit COverlayTextureVulkan(CDVDOverlaySpu* o);
  ~COverlayTextureVulkan() override = default;

  void Render(SRenderState& state) override {}
};

} // namespace OVERLAY
