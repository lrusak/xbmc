/*
 *  Copyright (C) 2023 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <string>
#include <vector>

#include "PlatformDefs.h"

struct in_addr;
struct sockaddr;

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

/*!
  \brief  IPv6/IPv4 compatible conversion of host IP address
  \param  struct sockaddr
  \return Function converts binary structure sockaddr to std::string.
          It can read sockaddr_in and sockaddr_in6, cast as (sockaddr*).
          IPv4 address is returned in the format x.x.x.x (where x is 0-255),
          IPv6 address is returned in it's canonised form.
          On error (or no IPv6/v4 valid input) empty string is returned.
  */
std::string GetIpStr(const sockaddr* sa);

//! \brief Formats an IPv4 address as x.x.x.x. Unlike inet_ntoa, it is safe to call from any thread.
std::string GetIpStr(const in_addr& address);

/*!
  \brief  convert prefix length of IPv4 address to IP mask representation
  \param  prefix length
  \return
  */
std::string GetMaskByPrefixLength(uint8_t prefixLength);
} // namespace KODI::NETWORK
