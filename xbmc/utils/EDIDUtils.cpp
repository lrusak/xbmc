/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "EDIDUtils.h"

#include "utils/log.h"

#include <map>

using namespace KODI::UTILS;

namespace
{
constexpr int EDID_BLOCK_LENGTH = 128;
constexpr int EDID_CEA_EXT_ID = 0x02;
constexpr int EDID_CEA_TAG_EXTENDED = 0x07;

/* CEA-861-G new EDID blocks for HDR */
constexpr int EDID_CEA_TAG_COLORIMETRY = 0x05;
constexpr int EDID_CEA_EXT_TAG_STATIC_METADATA = 0x06;

// Color enums is copied from linux include/drm/drm_color_mgmt.h (strangely not part of uapi)
enum drm_color_encoding {
  DRM_COLOR_YCBCR_BT601,
  DRM_COLOR_YCBCR_BT709,
  DRM_COLOR_YCBCR_BT2020,
};

std::array<std::string, 8> colorimetry_map =
{
  "xvYCC601",
  "xvYCC709",
  "sYCC601",
  "opYCC601",
  "opRGB",
  "BT2020cYCC",
  "BT2020YCC",
  "BT2020RGB",
};

std::array<std::string, 4> eotf_map =
{
  "Traditional gamma - SDR luminance range",
  "Traditional gamma - HDR luminance range",
  "SMPTE ST2084",
  "Hybrid Log-Gamma",
};


std::map<int, int> colorspace_map =
{
  {DRM_COLOR_YCBCR_BT2020, 7},
};
}

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
  const uint8_t* block = FindCEAExtentionBlock();

  if (!block)
    return std::vector<uint8_t>();

  const uint8_t* start = block + 4;
  const uint8_t* end = block + block[2] - 1;

  uint8_t length{0};
  for (const uint8_t* db = start; db < end; db += (length + 1))
  {
    length = db[0] & 0x1F;
    if ((db[0] >> 5) != EDID_CEA_TAG_EXTENDED)
      continue;

    if (db[1] == blockTag)
      return std::vector<uint8_t>(db + 2, db + 2 + length - 1);
  }

  return std::vector<uint8_t>();
}

bool CEDIDUtils::SupportsColorSpace(int colorSpace)
{
  std::vector<uint8_t> block = FindExtendedDataBlock(EDID_CEA_TAG_COLORIMETRY);

  if (block.size() >= 2)
  {
    std::string colorStr;
    for (size_t i = 0; i < colorimetry_map.size(); i++)
    {
      if (block[0] & (1 << i))
        colorStr.append("\n" + colorimetry_map[i]);
    }

    CLog::Log(LOGDEBUG, "CEDIDUtils:{} - supported connector colorimetry:{}", __FUNCTION__, colorStr);

    auto color = colorspace_map.find(colorSpace);
    if (color != colorspace_map.end())
      return block[0] & (1 << color->second);
  }

  CLog::Log(LOGDEBUG, "CEDIDUtils:{} - colorimetry not supported", __FUNCTION__);

  return false;
}

bool CEDIDUtils::SupportsEOTF(int eotf)
{
  std::vector<uint8_t> block = FindExtendedDataBlock(EDID_CEA_EXT_TAG_STATIC_METADATA);

  if (block.size() >= 3)
    CLog::Log(LOGDEBUG, "CEDIDUtils:{} - max luminance: {} ({} cd/m^2)", __FUNCTION__, static_cast<int>(block[2]), static_cast<int>(50.0 * pow(2, block[2] / 32.0)));

  if (block.size() >= 4)
    CLog::Log(LOGDEBUG, "CEDIDUtils:{} - maxFALL: {} ({} cd/m^2)", __FUNCTION__, static_cast<int>(block[3]), static_cast<int>(50.0 * pow(2, block[3] / 32.0)));

  if (block.size() >= 5)
    CLog::Log(LOGDEBUG, "CEDIDUtils:{} - min luminance: {} ({} cd/m^2)", __FUNCTION__, static_cast<int>(block[5]), static_cast<int>((50.0 * pow(2, block[2] / 32.0)) * pow(block[4] / 255.0, 2) / 100.0));

  if (block.size() >= 2)
  {
    for (int i = 0; i < 8; i++)
    {
      if (block[1] & (1 << i))
        CLog::Log(LOGDEBUG, "CEDIDUtils:{} - supported static metadata type {}", __FUNCTION__, i + 1);
    }

    std::string eotfStr;
    for (size_t i = 0; i < 6; i++)
    {
      if (block[0] & (1 << i))
      {
        if (i < eotf_map.size())
          eotfStr.append("\n" + eotf_map[i]);
        else
          eotfStr.append("\nunknown");
      }
    }

    CLog::Log(LOGDEBUG, "CEDIDUtils:{} - supported connector eotf:{}", __FUNCTION__, eotfStr);

    return block[0] & (1 << eotf);
  }

  CLog::Log(LOGDEBUG, "CEDIDUtils:{} - eotf not supported", __FUNCTION__);

  return false;
}

void CEDIDUtils::LogInfo()
{
  std::stringstream make;
  make << static_cast<char>(((m_edid[0x08 + 0] & 0x7C) >> 2) + '@');
  make << static_cast<char>(((m_edid[0x08 + 0] & 0x03) << 3) + ((m_edid[0x08 + 1] & 0xE0) >> 5) + '@');
  make << static_cast<char>((m_edid[0x08 + 1] & 0x1F) + '@');

  int model = static_cast<int>(m_edid[0x0A] + (m_edid[0x0B] << 8));

  CLog::Log(LOGNOTICE, "CEDIDUtils:{} - manufacturer '{}' model '{:x}'", __FUNCTION__, make.str(), model);
}
