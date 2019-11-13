/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "GUIFontTTF.h"
#include "TextureVulkan.h"

#include <string>
#include <vector>

class CGUIFontTTFVulkan : public CGUIFontTTF
{
public:
  explicit CGUIFontTTFVulkan(const std::string& strFileName);
  ~CGUIFontTTFVulkan() override = default;

  bool FirstBegin() override { return false; }
  void LastEnd() override {}

  CVertexBuffer CreateVertexBuffer(const std::vector<SVertex>& vertices) const override
  {
    return CVertexBuffer();
  }
  void DestroyVertexBuffer(CVertexBuffer& bufferHandle) const override {}

protected:
  std::unique_ptr<CTexture> ReallocTexture(unsigned int& newHeight) override { return std::make_unique<CTextureVulkan>(); }
  bool CopyCharToTexture(FT_BitmapGlyph bitGlyph,
                         unsigned int x1,
                         unsigned int y1,
                         unsigned int x2,
                         unsigned int y2) override
  {
    return false;
  }
  void DeleteHardwareTexture() override {}
};
