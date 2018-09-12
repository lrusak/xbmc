/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "VNCServer.h"

#include "ServiceBroker.h"
#include "application/AppInboundProtocol.h"
#include "input/mouse/MouseStat.h"
#include "utils/Map.h"
#include "utils/log.h"
#include "windowing/GraphicContext.h"
#include "windowing/WinSystem.h"

#include <rfb/keysym.h>

using namespace std::chrono_literals;

namespace
{

const auto keyMap = make_map<rfbKeySym, XBMCKey>({
    // Function keys before start of ASCII printable character range
    {XK_BackSpace, XBMCK_BACKSPACE},
    {XK_Tab, XBMCK_TAB},
    {XK_Clear, XBMCK_CLEAR},
    {XK_Return, XBMCK_RETURN},
    {XK_Pause, XBMCK_PAUSE},
    {XK_Escape, XBMCK_ESCAPE},

    // ASCII printable range - not included here

    // Function keys after end of ASCII printable character range
    {XK_Delete, XBMCK_DELETE},

    // Numeric keypad
    {XK_KP_0, XBMCK_KP0},
    {XK_KP_1, XBMCK_KP1},
    {XK_KP_2, XBMCK_KP2},
    {XK_KP_3, XBMCK_KP3},
    {XK_KP_4, XBMCK_KP4},
    {XK_KP_5, XBMCK_KP5},
    {XK_KP_6, XBMCK_KP6},
    {XK_KP_7, XBMCK_KP7},
    {XK_KP_8, XBMCK_KP8},
    {XK_KP_9, XBMCK_KP9},
    {XK_KP_Decimal, XBMCK_KP_PERIOD},
    {XK_KP_Divide, XBMCK_KP_DIVIDE},
    {XK_KP_Multiply, XBMCK_KP_MULTIPLY},
    {XK_KP_Subtract, XBMCK_KP_MINUS},
    {XK_KP_Add, XBMCK_KP_PLUS},
    {XK_KP_Enter, XBMCK_KP_ENTER},
    {XK_KP_Equal, XBMCK_KP_EQUALS},

    // Arrows + Home/End pad
    {XK_Up, XBMCK_UP},
    {XK_Down, XBMCK_DOWN},
    {XK_Right, XBMCK_RIGHT},
    {XK_Left, XBMCK_LEFT},
    {XK_Insert, XBMCK_INSERT},
    {XK_Home, XBMCK_HOME},
    {XK_End, XBMCK_END},
    {XK_Page_Up, XBMCK_PAGEUP},
    {XK_Page_Down, XBMCK_PAGEDOWN},

    // Key state modifier keys
    {XK_Num_Lock, XBMCK_NUMLOCK},
    {XK_Caps_Lock, XBMCK_CAPSLOCK},
    {XK_Scroll_Lock, XBMCK_SCROLLOCK},
    {XK_Shift_R, XBMCK_RSHIFT},
    {XK_Shift_L, XBMCK_LSHIFT},
    {XK_Control_R, XBMCK_RCTRL},
    {XK_Control_L, XBMCK_LCTRL},
    {XK_Alt_R, XBMCK_RALT},
    {XK_Alt_L, XBMCK_LALT},
    {XK_Meta_R, XBMCK_RMETA},
    {XK_Meta_L, XBMCK_LMETA},
    {XK_Super_R, XBMCK_RSUPER},
    {XK_Super_L, XBMCK_LSUPER},
    {XK_Multi_key, XBMCK_COMPOSE},

    // Miscellaneous function keys
    {XK_Help, XBMCK_HELP},
    {XK_Print, XBMCK_PRINT},
    {XK_Sys_Req, XBMCK_SYSREQ},
    {XK_Break, XBMCK_BREAK},
    {XK_Menu, XBMCK_MENU},
    {XK_EuroSign, XBMCK_EURO},
    {XK_Undo, XBMCK_UNDO},
});

constexpr XBMCKey XBMCKeyForKeysym(rfbKeySym sym)
{
  if (sym >= XK_a && sym <= XK_z)
  {
    return static_cast<XBMCKey>(sym);
  }
  else if (sym >= XK_space && sym <= XK_at)
  {
    return static_cast<XBMCKey>(sym);
  }
  else if (sym >= XK_F1 && sym <= XK_F15)
  {
    return static_cast<XBMCKey>(XBMCK_F1 + (static_cast<int>(sym) - XK_F1));
  }

  auto mapping = keyMap.find(sym);
  if (mapping != keyMap.cend())
    return mapping->second;

  return XBMCK_UNKNOWN;
}

struct ClientData
{
  int X{0};
  int Y{0};
};

} // namespace

CVNCServer::CVNCServer(int maxWidth, int maxHeight)
{
  m_width = maxWidth;
  m_height = maxHeight;

  m_screen = rfbGetScreen(nullptr, nullptr, m_width, m_height, 8, 3, 4);
  if (!m_screen)
    CLog::Log(LOGERROR, "CVNCServer::{} - failed to call rfbGetScreen", __FUNCTION__);

  m_screen->desktopName = "Kodi";
  m_screen->alwaysShared = TRUE;

  rfbPixelFormat pixelFormat;

  pixelFormat.bitsPerPixel = 32;
  pixelFormat.depth = 32;
  pixelFormat.bigEndian = 0;
  pixelFormat.trueColour = 1;
  pixelFormat.redMax = 255;
  pixelFormat.greenMax = 255;
  pixelFormat.blueMax = 255;
  pixelFormat.redShift = 16;
  pixelFormat.greenShift = 8;
  pixelFormat.blueShift = 0;

  // m_screen->httpDir = (char*)"/usr/share/novnc/vnc.html";

  m_screen->ptrAddEvent = OnPointer;
  m_screen->kbdAddEvent = OnKey;

  m_screen->newClientHook = ClientAdded;

  m_screen->serverFormat = pixelFormat;

  rfbInitServer(m_screen);
}

CVNCServer::~CVNCServer()
{
  rfbShutdownServer(m_screen, TRUE);
  rfbScreenCleanup(m_screen);
}

void CVNCServer::PumpEvents()
{
  rfbMarkRectAsModified(m_screen, 0, 0, m_width, m_height);
  rfbProcessEvents(m_screen, 0);
}

void CVNCServer::UpdateFrameBuffer(char* buffer)
{
  m_screen->frameBuffer = buffer;
}

void CVNCServer::ClientRemoved(rfbClientPtr client)
{
  CLog::Log(LOGDEBUG, "CVNCServer::{} - client removed: {}", __FUNCTION__, client->host);

  delete reinterpret_cast<ClientData*>(client->clientData);
}

rfbNewClientAction CVNCServer::ClientAdded(rfbClientPtr client)
{
  CLog::Log(LOGDEBUG, "CVNCServer::{} - client added: {}", __FUNCTION__, client->host);

  client->clientData = reinterpret_cast<void*>(new ClientData);
  client->clientGoneHook = ClientRemoved;
  return RFB_CLIENT_ACCEPT;
}

void CVNCServer::OnPointer(int buttonMask, int x, int y, rfbClientPtr client)
{
  CLog::Log(LOGDEBUG, "CVNCServer::{} - button: {} x: {} y: {}", __FUNCTION__, buttonMask, x, y);

  auto clientData = reinterpret_cast<ClientData*>(client->clientData);
  clientData->X = x;
  clientData->Y = y;

  // limit the mouse to the screen width
  clientData->X =
      std::min(CServiceBroker::GetWinSystem()->GetGfxContext().GetWidth(), clientData->X);
  clientData->X = std::max(0, clientData->X);

  // limit the mouse to the screen height
  clientData->Y =
      std::min(CServiceBroker::GetWinSystem()->GetGfxContext().GetHeight(), clientData->Y);
  clientData->Y = std::max(0, clientData->Y);

  XBMC_Event event = {};

  if (buttonMask > 0)
  {
    event.type = XBMC_MOUSEBUTTONDOWN;
    event.button.x = static_cast<uint16_t>(clientData->X);
    event.button.y = static_cast<uint16_t>(clientData->Y);

    switch (buttonMask)
    {
      case rfbButton1Mask:
        event.button.button = XBMC_BUTTON_LEFT;
        break;
      case rfbButton2Mask:
        event.button.button = XBMC_BUTTON_MIDDLE;
        break;
      case rfbButton3Mask:
        event.button.button = XBMC_BUTTON_RIGHT;
        break;
      case rfbButton4Mask:
        event.button.button = XBMC_BUTTON_WHEELUP;
        break;
      case rfbButton5Mask:
        event.button.button = XBMC_BUTTON_WHEELDOWN;
        break;
      default:
        break;
    }
  }
  else
  {
    event.type = XBMC_MOUSEMOTION;
    event.motion.x = static_cast<uint16_t>(clientData->X);
    event.motion.y = static_cast<uint16_t>(clientData->Y);
  }

  std::shared_ptr<CAppInboundProtocol> appPort = CServiceBroker::GetAppPort();
  if (appPort)
    appPort->OnEvent(event);

  if (buttonMask > 0)
  {
    event.type = XBMC_MOUSEBUTTONUP;

    if (appPort)
      appPort->OnEvent(event);
  }
}

void CVNCServer::OnKey(rfbBool down, rfbKeySym key, rfbClientPtr client)
{
  CLog::Log(LOGDEBUG, "CVNCServer::{} - down: {} key: {}", __FUNCTION__, static_cast<bool>(down),
            key);

  XBMC_Event event = {};

  event.type = static_cast<bool>(down) ? XBMC_KEYDOWN : XBMC_KEYUP;

  //! @todo: modifiers ???
  event.key.keysym.mod = XBMCKMOD_NONE;
  event.key.keysym.sym = XBMCKeyForKeysym(key);

  std::shared_ptr<CAppInboundProtocol> appPort = CServiceBroker::GetAppPort();
  if (appPort)
    appPort->OnEvent(event);
}
