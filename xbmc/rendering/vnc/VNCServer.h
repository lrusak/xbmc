/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <rfb/rfb.h>

class CVNCServer
{
public:
  CVNCServer(int maxWidth, int maxHeight);
  ~CVNCServer();

  void PumpEvents();

  void UpdateFrameBuffer(char* buffer);

private:
  static rfbNewClientAction ClientAdded(rfbClientPtr client);
  static void ClientRemoved(rfbClientPtr client);

  static void OnPointer(int buttonMask, int x, int y, rfbClientPtr client);
  static void OnKey(rfbBool down, rfbKeySym key, rfbClientPtr client);

  rfbScreenInfoPtr m_screen{nullptr};
  int m_width = 0;
  int m_height = 0;
};
