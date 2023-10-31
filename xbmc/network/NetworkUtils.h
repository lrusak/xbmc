/*
 *  Copyright (C) 2023 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

namespace KODI::NETWORK
{
/**
 * @brief Return true if magic packet was sent
 *
 * @param MAC address
 */
bool WakeOnLan(const char* mac);
} // namespace KODI::NETWORK
