/*
 *      Copyright (C) 2016-2017 Team Kodi
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
#pragma once

#include "cores/RetroPlayer/streams/RetroPlayerStreamTypes.h"

namespace KODI
{
namespace GAME
{
  class IGameRenderingCallback;

  class IGameRenderingCallback
  {
  public:
    virtual ~IGameRenderingCallback() = default;

    virtual bool Create() = 0;
    virtual void Destroy() = 0;
    virtual uintptr_t GetCurrentFramebuffer() = 0;
    virtual RETRO::ProcedureAddress GetProcAddress(const char *sym) = 0;
    virtual void RenderFrame() = 0;
  };

  class IGameInputCallback
  {
  public:
    virtual ~IGameInputCallback() = default;

    virtual void PollInput() = 0;
  };
}
}
