/*
 *  Copyright (C) 2023 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "NetworkUtils.h"

#include "network/SocketOptions.h"
#include "utils/ScopeGuard.h"
#include "utils/log.h"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstring>

#include <arpa/inet.h>
#include <netdb.h>
#ifndef TARGET_WINDOWS
#include <unistd.h>
#endif

namespace
{
#if defined(TARGET_WINDOWS)
using CSocketGuard = KODI::UTILS::CScopeGuard<SOCKET, INVALID_SOCKET, decltype(closesocket)>;
CSocketGuard GuardSocket(SOCKET sock)
{
  return CSocketGuard(closesocket, sock);
}
#else
using CSocketGuard = KODI::UTILS::CScopeGuard<int, -1, decltype(close)>;
CSocketGuard GuardSocket(int sock)
{
  return CSocketGuard(close, sock);
}
#endif
} // namespace

using namespace KODI;

std::optional<std::array<uint8_t, 6>> NETWORK::ParseMacAddress(std::string_view mac)
{
  // Six hex octets, each followed by ':' or '-' except the last
  std::array<uint8_t, 6> address;
  if (mac.size() != address.size() * 3 - 1)
    return std::nullopt;

  for (std::size_t i = 0; i < address.size(); ++i)
  {
    const char* octet = mac.data() + i * 3;
    const auto [end, error] = std::from_chars(octet, octet + 2, address[i], 16);
    if (error != std::errc{} || end != octet + 2)
      return std::nullopt;

    if (i + 1 < address.size() && *end != ':' && *end != '-')
      return std::nullopt;
  }

  return address;
}

bool NETWORK::WakeOnLan(std::string_view mac)
{
  const auto address = ParseMacAddress(mac);
  if (!address)
  {
    CLog::LogF(LOGERROR, "Invalid hardware address specified ({})", mac);
    return false;
  }

  const CSocketGuard packet = GuardSocket(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
  if (!packet)
  {
    CLog::LogF(LOGERROR, "Unable to create socket ({})", strerror(errno));
    return false;
  }

  sockaddr_in saddr{};
  saddr.sin_family = AF_INET;
  saddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
  saddr.sin_port = htons(9);

  const int broadcast = 1;
  if (setsockopt(packet, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&broadcast),
                 sizeof(broadcast)) == SOCKET_ERROR)
  {
    CLog::LogF(LOGERROR, "Unable to set socket options ({})", strerror(errno));
    return false;
  }

  // The magic packet is 6 x 0xff followed by the MAC address 16 times
  std::array<uint8_t, 6 + 16 * 6> magicPacket;
  std::fill_n(magicPacket.begin(), 6, 0xff);
  for (auto it = magicPacket.begin() + 6; it != magicPacket.end(); it += address->size())
    std::ranges::copy(*address, it);

  if (sendto(packet, reinterpret_cast<const char*>(magicPacket.data()), magicPacket.size(), 0,
             reinterpret_cast<const sockaddr*>(&saddr), sizeof(saddr)) < 0)
  {
    CLog::LogF(LOGERROR, "Unable to send magic packet ({})", strerror(errno));
    return false;
  }

  CLog::LogF(LOGINFO, "Magic packet sent to '{}'", mac);
  return true;
}

std::vector<SOCKET> NETWORK::CreateTCPServerSocket(const int port,
                                                   const bool bindLocal,
                                                   const int backlog,
                                                   const char* callerName)
{
  std::vector<SOCKET> sockets;
  struct addrinfo* results = nullptr;

  std::string sPort = std::to_string(port);
  struct addrinfo hints = {};
  hints.ai_family = PF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  hints.ai_protocol = 0;

  int error = getaddrinfo(bindLocal ? "localhost" : nullptr, sPort.c_str(), &hints, &results);
  if (error != 0)
    return sockets;

  for (struct addrinfo* result = results; result != nullptr; result = result->ai_next)
  {
    SOCKET sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sock == INVALID_SOCKET)
      continue;

    SetReusePort(sock);
    SetIPv6Only(sock, 1);

    if (bind(sock, result->ai_addr, result->ai_addrlen) != 0)
    {
      closesocket(sock);
      CLog::Log(LOGDEBUG, "{} Server: Failed to bind {} serversocket", callerName,
                result->ai_family == AF_INET6 ? "IPv6" : "IPv4");
      continue;
    }

    if (listen(sock, backlog) == 0)
      sockets.push_back(sock);
    else
    {
      closesocket(sock);
      CLog::Log(LOGERROR, "{} Server: Failed to set listen", callerName);
    }
  }
  freeaddrinfo(results);

  if (sockets.empty())
    CLog::Log(LOGERROR, "{} Server: Failed to create serversocket(s)", callerName);

  return sockets;
}

std::string NETWORK::GetIpStr(const struct sockaddr* sa)
{
  if (!sa)
    return "";

  std::string buffer;
  switch (sa->sa_family)
  {
    case AF_INET:
    {
      buffer.resize(INET_ADDRSTRLEN);
      if (!inet_ntop(AF_INET, &reinterpret_cast<const struct sockaddr_in*>(sa)->sin_addr,
                     buffer.data(), buffer.length()))
        return "";
      break;
    }
    case AF_INET6:
    {
      buffer.resize(INET6_ADDRSTRLEN);
      if (!inet_ntop(AF_INET6, &reinterpret_cast<const struct sockaddr_in6*>(sa)->sin6_addr,
                     buffer.data(), buffer.length()))
        return "";
      break;
    }
    default:
      return "";
  }

  buffer.resize(std::strlen(buffer.c_str()));
  return buffer;
}

std::string NETWORK::GetIpStr(const in_addr& address)
{
  char buffer[INET_ADDRSTRLEN];
  if (!inet_ntop(AF_INET, &address, buffer, sizeof(buffer)))
    return "";

  return buffer;
}

std::string NETWORK::GetMaskByPrefixLength(uint8_t prefixLength)
{
  if (prefixLength > 32)
    return "";

  // Shifting a 32-bit value by 32 is undefined, so /0 is handled on its own.
  const uint32_t mask = prefixLength == 0 ? 0 : ~uint32_t{0} << (32 - prefixLength);

  struct sockaddr_in sa;
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = htonl(mask);

  return NETWORK::GetIpStr(reinterpret_cast<struct sockaddr*>(&sa));
}
