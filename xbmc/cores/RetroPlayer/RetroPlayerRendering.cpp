/*
 *      Copyright (C) 2012-2017 Team Kodi
 *      http://kodi.tv
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this Program; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "RetroPlayerRendering.h"
#include "cores/RetroPlayer/process/RPProcessInfo.h"
#include "cores/RetroPlayer/rendering/RPRenderManager.h"

#include "windowing/X11/WinSystemX11GLContext.h"
#include "rendering/RenderSystem.h"
#include "ServiceBroker.h"
#include "utils/log.h"

using namespace KODI;
using namespace RETRO;

CRetroPlayerRendering::CRetroPlayerRendering(CRPRenderManager& renderManager, CRPProcessInfo& processInfo) :
  m_renderManager(renderManager),
  m_processInfo(processInfo),
  m_width(640),
  m_height(480)
{
}

bool CRetroPlayerRendering::Create()
{
  m_processInfo.SetVideoPixelFormat(AV_PIX_FMT_BGRA);
  m_processInfo.SetVideoDimensions(m_width, m_height);

  if (!m_renderManager.Configure(AV_PIX_FMT_BGRA, m_width, m_height, 0))
    return false;

  return m_renderManager.Create();
}

void CRetroPlayerRendering::Destroy()
{
  //! @todo
}

void CRetroPlayerRendering::RenderFrame()
{
  m_renderManager.RenderFrame();
}

uintptr_t CRetroPlayerRendering::GetCurrentFramebuffer()
{
  return m_renderManager.GetCurrentFramebuffer();
}
