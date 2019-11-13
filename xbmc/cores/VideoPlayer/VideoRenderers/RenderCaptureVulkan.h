/*
 *  Copyright (C) 2005-2021 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "RenderCapture.h"

class CRenderCaptureVulkan : public CRenderCaptureBase
{
public:
  CRenderCaptureVulkan() = default;
  ~CRenderCaptureVulkan() override = default;

  int GetCaptureFormat() { return 0; }

  void BeginRender() {}
  void EndRender() {}
  void ReadOut() {}
};

//used instead of typedef CRenderCaptureGL CRenderCapture
//since C++ doesn't allow you to forward declare a typedef
class CRenderCapture : public CRenderCaptureVulkan
{
public:
  CRenderCapture() = default;
};
