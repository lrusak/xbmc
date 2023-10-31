/*
 *  Copyright (C) 2023 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "NetworkUtils.h"

#include "utils/log.h"

#include <arpa/inet.h>

#include "PlatformDefs.h"

namespace
{

/* slightly modified in_ether taken from the etherboot project (http://sourceforge.net/projects/etherboot) */
bool in_ether(const char* bufp, unsigned char* addr)
{
  if (strlen(bufp) != 17)
    return false;

  char c;
  const char* orig;
  unsigned char* ptr = addr;
  unsigned val;

  int i = 0;
  orig = bufp;

  while ((*bufp != '\0') && (i < 6))
  {
    val = 0;
    c = *bufp++;

    if (isdigit(c))
      val = c - '0';
    else if (c >= 'a' && c <= 'f')
      val = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F')
      val = c - 'A' + 10;
    else
      return false;

    val <<= 4;
    c = *bufp;
    if (isdigit(c))
      val |= c - '0';
    else if (c >= 'a' && c <= 'f')
      val |= c - 'a' + 10;
    else if (c >= 'A' && c <= 'F')
      val |= c - 'A' + 10;
    else if (c == ':' || c == '-' || c == 0)
      val >>= 4;
    else
      return false;

    if (c != 0)
      bufp++;

    *ptr++ = (unsigned char)(val & 0377);
    i++;

    if (*bufp == ':' || *bufp == '-')
      bufp++;
  }

  if (bufp - orig != 17)
    return false;

  return true;
}

} // namespace

using namespace KODI;

bool NETWORK::WakeOnLan(const char* mac)
{
  int i, j, packet;
  unsigned char ethaddr[8];
  unsigned char buf[128];
  unsigned char* ptr;

  // Fetch the hardware address
  if (!in_ether(mac, ethaddr))
  {
    CLog::Log(LOGERROR, "{} - Invalid hardware address specified ({})", __FUNCTION__, mac);
    return false;
  }

  // Setup the socket
  if ((packet = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
  {
    CLog::Log(LOGERROR, "{} - Unable to create socket ({})", __FUNCTION__, strerror(errno));
    return false;
  }

  // Set socket options
  struct sockaddr_in saddr;
  saddr.sin_family = AF_INET;
  saddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
  saddr.sin_port = htons(9);

  unsigned int value = 1;
  if (setsockopt(packet, SOL_SOCKET, SO_BROADCAST, (char*)&value, sizeof(unsigned int)) ==
      SOCKET_ERROR)
  {
    CLog::Log(LOGERROR, "{} - Unable to set socket options ({})", __FUNCTION__, strerror(errno));
    closesocket(packet);
    return false;
  }

  // Build the magic packet (6 x 0xff + 16 x MAC address)
  ptr = buf;
  for (i = 0; i < 6; i++)
    *ptr++ = 0xff;

  for (j = 0; j < 16; j++)
    for (i = 0; i < 6; i++)
      *ptr++ = ethaddr[i];

  // Send the magic packet
  if (sendto(packet, (char*)buf, 102, 0, (struct sockaddr*)&saddr, sizeof(saddr)) < 0)
  {
    CLog::Log(LOGERROR, "{} - Unable to send magic packet ({})", __FUNCTION__, strerror(errno));
    closesocket(packet);
    return false;
  }

  closesocket(packet);
  CLog::Log(LOGINFO, "{} - Magic packet send to '{}'", __FUNCTION__, mac);
  return true;
}
