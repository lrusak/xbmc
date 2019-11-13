/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GUITextureVulkan.h"

#include "Texture.h"

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
}

CGUITextureVulkan* CGUITextureVulkan::Clone() const
{
  return new CGUITextureVulkan(*this);
}

void CGUITextureVulkan::Begin(UTILS::COLOR::Color color)
{
}

void CGUITextureVulkan::End()
{
}

void CGUITextureVulkan::Draw(
    float* x, float* y, float* z, const CRect& texture, const CRect& diffuse, int orientation)
{
}

void CGUITextureVulkan::DrawQuad(const CRect& rect,
                                 UTILS::COLOR::Color color,
                                 CTexture* texture,
                                 const CRect* texCoords)
{
}
