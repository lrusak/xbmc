/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "EDIDUtils.h"

#include "utils/log.h"

#include <bitset>
#include <cmath>
#include <map>
#include <tuple>

using namespace KODI::UTILS;

namespace
{
constexpr int EDID_BLOCK_LENGTH = 128;
constexpr int EDID_CEA_EXT_ID = 0x02;
constexpr int EDID_CEA_TAG_EXTENDED = 0x07;

/* CEA-861-G new EDID blocks for HDR */
constexpr int EDID_CEA_TAG_COLORIMETRY = 0x05;
constexpr int EDID_CEA_EXT_TAG_STATIC_METADATA = 0x06;

std::array<std::string, 4> eotf_map = {
    "Traditional gamma - SDR luminance range",
    "Traditional gamma - HDR luminance range",
    "SMPTE ST2084",
    "Hybrid Log-Gamma",
};

// colorimetry definitions are copied from linux include/drm/drm_connector.h (not part of uapi yet)
/* CEA 861 Extended Colorimetry Options */
constexpr int DRM_MODE_COLORIMETRY_XVYCC_601{3};
constexpr int DRM_MODE_COLORIMETRY_XVYCC_709{4};
constexpr int DRM_MODE_COLORIMETRY_SYCC_601{5};
constexpr int DRM_MODE_COLORIMETRY_OPYCC_601{6};
constexpr int DRM_MODE_COLORIMETRY_OPRGB{7};
constexpr int DRM_MODE_COLORIMETRY_BT2020_CYCC{8};
constexpr int DRM_MODE_COLORIMETRY_BT2020_RGB{9};
constexpr int DRM_MODE_COLORIMETRY_BT2020_YCC{10};

std::array<std::pair<uint8_t, std::string>, 8> colorimetry_map = {{
    {DRM_MODE_COLORIMETRY_XVYCC_601, "XVYCC_601"},
    {DRM_MODE_COLORIMETRY_XVYCC_709, "XVYCC_709"},
    {DRM_MODE_COLORIMETRY_SYCC_601, "SYCC_601"},
    {DRM_MODE_COLORIMETRY_OPYCC_601, "opYCC_601"},
    {DRM_MODE_COLORIMETRY_OPRGB, "opRGB"},
    {DRM_MODE_COLORIMETRY_BT2020_CYCC, "BT2020_CYCC"},
    {DRM_MODE_COLORIMETRY_BT2020_YCC, "BT2020_YCC"},
    {DRM_MODE_COLORIMETRY_BT2020_RGB, "BT2020_RGB"},
}};

// Color enums is copied from linux include/drm/drm_color_mgmt.h (strangely not part of uapi)
enum drm_color_encoding
{
  DRM_COLOR_YCBCR_BT601,
  DRM_COLOR_YCBCR_BT709,
  DRM_COLOR_YCBCR_BT2020,
};

} // namespace

const uint8_t* CEDIDUtils::FindCEAExtentionBlock()
{
  const uint8_t* extension = nullptr;
  for (int block = 0; block < m_edid[126]; block++)
  {
    extension = &m_edid[EDID_BLOCK_LENGTH * (block + 1)];

    if (block == m_edid[126])
      return nullptr;

    if (extension[0] == EDID_CEA_EXT_ID)
      return extension;
  }

  return nullptr;
}

std::vector<uint8_t> CEDIDUtils::FindExtendedDataBlock(uint32_t blockTag)
{
  auto block = FindCEAExtentionBlock();

  std::vector<uint8_t> dataBlock;

  if (block)
  {
    const uint8_t* start = block + 4;
    const uint8_t* end = block + block[2] - 1;

    uint8_t length{0};
    for (const uint8_t* db = start; db < end; db += (length + 1))
    {
      length = db[0] & 0x1F;
      if ((db[0] >> 5) != EDID_CEA_TAG_EXTENDED)
        continue;

      if (db[1] == blockTag)
      {
        dataBlock = std::vector<uint8_t>(db + 2, db + 2 + length - 1);
        break;
      }
    }
  }

  return dataBlock;
}

void CEDIDUtils::LogSupportedColorimetry()
{
  if (m_edid.empty())
    return;

  auto block = FindExtendedDataBlock(EDID_CEA_TAG_COLORIMETRY);

  if (block.size() >= 2)
  {
    std::string colorStr;

    constexpr size_t maxColorimetryTypes{8};
    static_assert(maxColorimetryTypes == colorimetry_map.size());
    std::bitset<maxColorimetryTypes> supportedColorimetryTypes{block[0]};

    for (size_t i = 0; i < maxColorimetryTypes; i++)
    {
      if (supportedColorimetryTypes[i])
        colorStr.append("\n" + colorimetry_map[i].second);
    }

    CLog::Log(LOGDEBUG, "CEDIDUtils:{} - supported connector colorimetry:{}", __FUNCTION__,
              colorStr);
  }
}

bool CEDIDUtils::SupportsColorimetry(uint8_t colorimetry)
{
  if (m_edid.empty())
    return false;

  auto block = FindExtendedDataBlock(EDID_CEA_TAG_COLORIMETRY);

  if (block.size() >= 2)
  {
    constexpr size_t maxColorimetryTypes{8};
    static_assert(maxColorimetryTypes == colorimetry_map.size());
    std::bitset<maxColorimetryTypes> supportedColorimetryTypes{block[0]};

    for (size_t i = 0; i < maxColorimetryTypes; i++)
    {
      if (colorimetry_map[i].first == colorimetry)
      {
        if (supportedColorimetryTypes[i])
          return true;

        CLog::Log(LOGDEBUG, "CEDIDUtils::{} - edid does not support requested colorimetry: {}",
                  __FUNCTION__, colorimetry_map[i].second);
      }
    }
  }

  return false;
}

void CEDIDUtils::LogSupportedEOTF()
{
  if (m_edid.empty())
    return;

  auto block = FindExtendedDataBlock(EDID_CEA_EXT_TAG_STATIC_METADATA);

  if (block.size() >= 2)
  {
    constexpr size_t maxStaticMetadataTypes{8};
    std::bitset<maxStaticMetadataTypes> supportedMetadataTypes{block[1]};
    for (size_t i = 0; i < maxStaticMetadataTypes; i++)
    {
      if (supportedMetadataTypes[i])
        CLog::Log(LOGDEBUG, "CEDIDUtils:{} - supported static metadata type {}", __FUNCTION__,
                  i + 1);
    }

    std::string eotfStr;
    constexpr size_t maxEotfs{4};
    std::bitset<maxEotfs> supportedEotfs{block[0]};
    for (size_t i = 0; i < maxEotfs; i++)
    {
      if (supportedEotfs[i])
        eotfStr.append("\n" + eotf_map[i]);
    }

    CLog::Log(LOGDEBUG, "CEDIDUtils:{} - supported connector eotf:{}", __FUNCTION__, eotfStr);
  }
}

bool CEDIDUtils::SupportsEOTF(uint8_t eotf)
{
  if (m_edid.empty())
    return false;

  auto block = FindExtendedDataBlock(EDID_CEA_EXT_TAG_STATIC_METADATA);

  if (block.size() >= 2)
  {
    constexpr size_t maxEotfs{4};
    std::bitset<maxEotfs> supportedEotfs{block[0]};
    if (supportedEotfs[eotf])
      return true;
  }

  CLog::Log(LOGDEBUG, "CEDIDUtils:{} - edid does not support requested eotf: {}", __FUNCTION__,
            eotf_map[eotf]);

  return false;
}

void CEDIDUtils::LogSupportedLuminance()
{
  if (m_edid.empty())
    return;

  auto block = FindExtendedDataBlock(EDID_CEA_EXT_TAG_STATIC_METADATA);

  if (block.size() >= 3)
    CLog::Log(LOGDEBUG, "CEDIDUtils:{} - max luminance: {} ({} cd/m^2)", __FUNCTION__,
              static_cast<int>(block[2]), static_cast<int>(50.0 * pow(2, block[2] / 32.0)));

  if (block.size() >= 4)
    CLog::Log(LOGDEBUG, "CEDIDUtils:{} - maxFALL: {} ({} cd/m^2)", __FUNCTION__,
              static_cast<int>(block[3]), static_cast<int>(50.0 * pow(2, block[3] / 32.0)));

  if (block.size() >= 5)
    CLog::Log(
        LOGDEBUG, "CEDIDUtils:{} - min luminance: {} ({} cd/m^2)", __FUNCTION__,
        static_cast<int>(block[5]),
        static_cast<int>((50.0 * pow(2, block[2] / 32.0)) * pow(block[4] / 255.0, 2) / 100.0));
}

void CEDIDUtils::ClampLuminance(std::tuple<int, int, int>& luminance)
{
  if (!m_edid.empty())
  {
    auto block = FindExtendedDataBlock(EDID_CEA_EXT_TAG_STATIC_METADATA);

    int max{0};
    if (block.size() >= 3)
      max = std::min(static_cast<int>(50.0 * pow(2, block[2] / 32.0)), std::get<0>(luminance));

    int avg{0};
    if (block.size() >= 4)
      avg = std::min(static_cast<int>(50.0 * pow(2, block[3] / 32.0)), std::get<1>(luminance));

    int min{0};
    if (block.size() >= 5)
      min = std::max(static_cast<int>(max * pow(block[4] / 255.0, 2) / 100.0),
                     std::get<2>(luminance));

    luminance = {max, avg, min};
  }
}

void CEDIDUtils::LogInfo()
{
  if (m_edid.empty())
    return;

  std::stringstream make;
  make << static_cast<char>(((m_edid[0x08 + 0] & 0x7C) >> 2) + '@');
  make << static_cast<char>(((m_edid[0x08 + 0] & 0x03) << 3) + ((m_edid[0x08 + 1] & 0xE0) >> 5) +
                            '@');
  make << static_cast<char>((m_edid[0x08 + 1] & 0x1F) + '@');

  int model = static_cast<int>(m_edid[0x0A] + (m_edid[0x0B] << 8));

  CLog::Log(LOGNOTICE, "CEDIDUtils:{} - manufacturer '{}' model '{:x}'", __FUNCTION__, make.str(),
            model);

  LogSupportedColorimetry();
  LogSupportedEOTF();
  LogSupportedLuminance();
}
