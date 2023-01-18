/*
 *  Copyright (C) 2010-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "Utils/AEAudioFormat.h"
#include "Utils/AEDeviceInfo.h"

#include <map>
#include <stdint.h>
#include <string>
#include <vector>

class IAESink;

namespace AE
{

constexpr auto DRIVER_DEVICE_DELIMITER = ":";

/*! This type is used to hold the DRIVER:DEVICE pair that is saved
    in the SETTING_AUDIOOUTPUT_AUDIODEVICE setting. The setting needs
    to be parsed as it is just a string. The ParseDevice method can be
    used to transform std::string into AEDevice.
 */
using AEDevice = std::pair<std::string, std::string>;

struct AESinkInfo
{
  std::string m_sinkName;
  AEDeviceInfoList m_deviceInfoList;
};

typedef IAESink* (*CreateSink)(const std::string& device, AEAudioFormat& desiredFormat);
typedef void (*Enumerate)(AEDeviceInfoList &list, bool force);
typedef void (*Cleanup)();

struct AESinkRegEntry
{
  std::string sinkName;
  CreateSink createFunc = nullptr;
  Enumerate enumerateFunc = nullptr;
  Cleanup cleanupFunc = nullptr;
};

class CAESinkFactory
{
public:
  static void RegisterSink(const AESinkRegEntry& regEntry);
  static void ClearSinks();
  static bool HasSinks();

  static AEDevice ParseDevice(const std::string& device);
  static IAESink* Create(const AEDevice& aeDevice, AEAudioFormat& desiredFormat);
  static void EnumerateEx(std::vector<AESinkInfo>& list, bool force, const std::string& driver);
  static void Cleanup();

protected:
  static std::map<std::string, AESinkRegEntry> m_AESinkRegEntry;
};

}
