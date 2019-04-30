/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <cstdint>
#include <vector>

namespace KODI
{
namespace UTILS
{

class CEDIDUtils
{
public:
  CEDIDUtils(std::vector<uint8_t> edid) : m_edid(edid) {}
  ~CEDIDUtils() = default;

  bool SupportsColorimetry(uint8_t colorimetry);
  bool SupportsEOTF(uint8_t eotf);
  void ClampLuminance(std::tuple<int, int, int>& luminance);
  void LogInfo();

private:
  const uint8_t* FindCEAExtentionBlock();
  std::vector<uint8_t> FindExtendedDataBlock(uint32_t blockTag);

  void LogSupportedColorimetry();
  void LogSupportedEOTF();
  void LogSupportedLuminance();

  std::vector<uint8_t> m_edid;
};

} // namespace UTILS
} // namespace KODI
