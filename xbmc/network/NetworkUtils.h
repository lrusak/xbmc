/*
 *  Copyright (C) 2023 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <vector>

#include "PlatformDefs.h"

namespace KODI::NETWORK
{
/**
 * @brief Return true if magic packet was sent
 *
 * @param MAC address
 */
bool WakeOnLan(const char* mac);

/**
 * @brief Creates, binds and listens tcp sockets on the desired port
 *
 * @param port
 * @param bindLocal True to bind to localhost only
 * @param backlog
 * @param callerName
 * @return std::vector<SOCKET> list of sockets
 */
std::vector<SOCKET> CreateTCPServerSocket(const int port,
                                          const bool bindLocal,
                                          const int backlog,
                                          const char* callerName);
} // namespace KODI::NETWORK
