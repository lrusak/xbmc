/*
 *  Copyright (C) 2010-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "AESinkFactory.h"

#include "Interfaces/AESink.h"
#include "ServiceBroker.h"
#include "utils/StringUtils.h"
#include "utils/log.h"

#include <algorithm>

using namespace AE;

std::map<std::string, AESinkRegEntry> CAESinkFactory::m_AESinkRegEntry;

void CAESinkFactory::RegisterSink(const AESinkRegEntry& regEntry)
{
  m_AESinkRegEntry[regEntry.sinkName] = regEntry;

  IAE *ae = CServiceBroker::GetActiveAE();
  if (ae)
    ae->DeviceChange();
}

void CAESinkFactory::ClearSinks()
{
  m_AESinkRegEntry.clear();
}

bool CAESinkFactory::HasSinks()
{
  return !m_AESinkRegEntry.empty();
}

AEDevice CAESinkFactory::ParseDevice(const std::string& device)
{
  AEDevice aeDevice;

  std::vector<std::string> parsed = StringUtils::Split(device, DRIVER_DEVICE_DELIMITER);
  if (parsed.size() > 1)
  {
    aeDevice = std::make_pair(parsed[0], parsed[1]);
  }
  else
  {
    aeDevice = std::make_pair("", device);
  }

  return aeDevice;
}

IAESink* CAESinkFactory::Create(const AEDevice& aeDevice, AEAudioFormat& desiredFormat)
{
  auto& [driver, device] = aeDevice;

  AEAudioFormat tmpFormat = desiredFormat;
  IAESink* sink;

  for (const auto& [name, entry] : m_AESinkRegEntry)
  {
    if (driver != entry.sinkName)
      continue;

    sink = entry.createFunc(device, tmpFormat);
    if (sink)
    {
      desiredFormat = tmpFormat;
      return sink;
    }
  }
  return nullptr;
}

void CAESinkFactory::EnumerateEx(std::vector<AESinkInfo>& list,
                                 bool force,
                                 const std::string& driver)
{
  AESinkInfo info;

  for (const auto& [name, entry] : m_AESinkRegEntry)
  {
    if (!driver.empty() && driver != entry.sinkName)
      continue;

    info.m_deviceInfoList.clear();
    info.m_sinkName = entry.sinkName;
    entry.enumerateFunc(info.m_deviceInfoList, force);

    if (!info.m_deviceInfoList.empty())
      list.push_back(info);
  }
}

void CAESinkFactory::Cleanup()
{
  for (const auto& [name, entry] : m_AESinkRegEntry)
  {
    if (entry.cleanupFunc)
      entry.cleanupFunc();
  }
}
