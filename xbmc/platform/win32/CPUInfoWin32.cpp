/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "CPUInfoWin32.h"

#include "platform/win32/CharsetConverter.h"
#include "ServiceBroker.h"
#include "settings/AdvancedSettings.h"
#include "settings/SettingsComponent.h"
#include "utils/StringUtils.h"
#include "utils/SysfsUtils.h"
#include "utils/Temperature.h"

#include <algorithm>

#include <intrin.h>
#include <Pdh.h>
#include <PdhMsg.h>

#pragma comment(lib, "Pdh.lib")

namespace
{
const unsigned int CPUINFO_EAX{0};
const unsigned int CPUINFO_EBX{1};
const unsigned int CPUINFO_ECX{2};
const unsigned int CPUINFO_EDX{3};
}

std::shared_ptr<CCPUInfo> CCPUInfo::GetCPUInfo()
{
  return std::make_shared<CCPUInfoWin32>();
}

CCPUInfoWin32::CCPUInfoWin32()
{
  HKEY hKeyCpuRoot;

  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor", 0, KEY_READ, &hKeyCpuRoot) == ERROR_SUCCESS)
  {
    DWORD num = 0;
    wchar_t subKeyName[200]; // more than enough
    DWORD subKeyNameLen = sizeof(subKeyName) / sizeof(wchar_t);
    while (RegEnumKeyExW(hKeyCpuRoot, num++, subKeyName, &subKeyNameLen, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
    {
      HKEY hCpuKey;
      if (RegOpenKeyExW(hKeyCpuRoot, subKeyName, 0, KEY_QUERY_VALUE, &hCpuKey) == ERROR_SUCCESS)
      {
        CoreInfo cpuCore;
        if (swscanf_s(subKeyName, L"%i", &cpuCore.m_id) != 1)
          cpuCore.m_id = num - 1;
        wchar_t buf[300]; // more than enough
        DWORD bufSize = sizeof(buf);
        DWORD valType;
        if (RegQueryValueExW(hCpuKey, L"ProcessorNameString", nullptr, &valType, LPBYTE(buf), &bufSize) == ERROR_SUCCESS &&
            valType == REG_SZ)
        {
          cpuCore.m_strModel = FromW(buf, bufSize / sizeof(wchar_t));
          cpuCore.m_strModel = cpuCore.m_strModel.substr(0, cpuCore.m_strModel.find(char(0))); // remove extra null terminations
          StringUtils::RemoveDuplicatedSpacesAndTabs(cpuCore.m_strModel);
          StringUtils::Trim(cpuCore.m_strModel);
        }
        bufSize = sizeof(buf);
        if (RegQueryValueExW(hCpuKey, L"VendorIdentifier", nullptr, &valType, LPBYTE(buf), &bufSize) == ERROR_SUCCESS &&
            valType == REG_SZ)
        {
          cpuCore.m_strVendor = FromW(buf, bufSize / sizeof(wchar_t));
          cpuCore.m_strVendor = cpuCore.m_strVendor.substr(0, cpuCore.m_strVendor.find(char(0))); // remove extra null terminations
        }
        DWORD mhzVal;
        bufSize = sizeof(mhzVal);
        if (RegQueryValueExW(hCpuKey, L"~MHz", nullptr, &valType, LPBYTE(&mhzVal), &bufSize) == ERROR_SUCCESS &&
            valType == REG_DWORD)
          cpuCore.m_fSpeed = double(mhzVal);

        RegCloseKey(hCpuKey);

        m_cpuCores.push_back(cpuCore);
      }
      subKeyNameLen = sizeof(subKeyName) / sizeof(wchar_t); // restore length value
    }
    RegCloseKey(hKeyCpuRoot);
  }

  SYSTEM_INFO siSysInfo;
  GetNativeSystemInfo(&siSysInfo);
  m_cpuCount = siSysInfo.dwNumberOfProcessors;

  if (PdhOpenQueryW(nullptr, 0, &m_cpuQueryFreq) == ERROR_SUCCESS)
  {
    if (PdhAddEnglishCounterW(m_cpuQueryFreq, L"\\Processor Information(0,0)\\Processor Frequency", 0, &m_cpuFreqCounter) != ERROR_SUCCESS)
      m_cpuFreqCounter = nullptr;
  }
  else
    m_cpuQueryFreq = nullptr;

  if (PdhOpenQueryW(nullptr, 0, &m_cpuQueryLoad) == ERROR_SUCCESS)
  {
    for (size_t i = 0; i < m_cores.size(); i++)
    {
      if (PdhAddEnglishCounterW(m_cpuQueryLoad, StringUtils::Format(L"\\Processor(%d)\\%% Idle Time", int(i)).c_str(), 0, &m_cores[i].m_coreCounter) != ERROR_SUCCESS)
        m_cores[i].m_coreCounter = nullptr;
    }
  }
  else
    m_cpuQueryLoad = nullptr;

#ifndef _M_ARM
  int CPUInfo[4]; // receives EAX, EBX, ECD and EDX in that order

  __cpuid(CPUInfo, 0);
  int MaxStdInfoType = CPUInfo[0];

  if (MaxStdInfoType >= CPUID_INFOTYPE_STANDARD)
  {
    __cpuid(CPUInfo, CPUID_INFOTYPE_STANDARD);
    if (CPUInfo[CPUINFO_EDX] & CPUID_00000001_EDX_MMX)
      m_cpuFeatures |= CPU_FEATURE_MMX;
    if (CPUInfo[CPUINFO_EDX] & CPUID_00000001_EDX_SSE)
      m_cpuFeatures |= CPU_FEATURE_SSE;
    if (CPUInfo[CPUINFO_EDX] & CPUID_00000001_EDX_SSE2)
      m_cpuFeatures |= CPU_FEATURE_SSE2;
    if (CPUInfo[CPUINFO_ECX] & CPUID_00000001_ECX_SSE3)
      m_cpuFeatures |= CPU_FEATURE_SSE3;
    if (CPUInfo[CPUINFO_ECX] & CPUID_00000001_ECX_SSSE3)
      m_cpuFeatures |= CPU_FEATURE_SSSE3;
    if (CPUInfo[CPUINFO_ECX] & CPUID_00000001_ECX_SSE4)
      m_cpuFeatures |= CPU_FEATURE_SSE4;
    if (CPUInfo[CPUINFO_ECX] & CPUID_00000001_ECX_SSE42)
      m_cpuFeatures |= CPU_FEATURE_SSE42;
  }

  __cpuid(CPUInfo, CPUID_INFOTYPE_EXTENDED_IMPLEMENTED);
  if (CPUInfo[0] >= CPUID_INFOTYPE_EXTENDED)
  {
    __cpuid(CPUInfo, CPUID_INFOTYPE_EXTENDED);

    if (CPUInfo[CPUINFO_EDX] & CPUID_80000001_EDX_MMX)
      m_cpuFeatures |= CPU_FEATURE_MMX;
    if (CPUInfo[CPUINFO_EDX] & CPUID_80000001_EDX_MMX2)
      m_cpuFeatures |= CPU_FEATURE_MMX2;
    if (CPUInfo[CPUINFO_EDX] & CPUID_80000001_EDX_3DNOW)
      m_cpuFeatures |= CPU_FEATURE_3DNOW;
    if (CPUInfo[CPUINFO_EDX] & CPUID_80000001_EDX_3DNOWEXT)
      m_cpuFeatures |= CPU_FEATURE_3DNOWEXT;
  }
#endif // ! _M_ARM

  // Set MMX2 when SSE is present as SSE is a superset of MMX2 and Intel doesn't set the MMX2 cap
  if (m_cpuFeatures & CPU_FEATURE_SSE)
    m_cpuFeatures |= CPU_FEATURE_MMX2;
}

CCPUInfoWin32::~CCPUInfoWin32()
{
  if (m_cpuQueryFreq)
    PdhCloseQuery(m_cpuQueryFreq);

  if (m_cpuQueryLoad)
    PdhCloseQuery(m_cpuQueryLoad);
}

int CCPUInfoWin32::GetUsedPercentage()
{
  int result = 0;

  if (!m_nextUsedReadTime.IsTimePast())
    return m_lastUsedPercentage;

  FILETIME idleTime;
  FILETIME kernelTime;
  FILETIME userTime;
  if (GetSystemTimes(&idleTime, &kernelTime, &userTime) == 0)
    return false;

  idle = (static_cast<uint64_t>(idleTime.dwHighDateTime) << 32) + static_cast<uint64_t>(idleTime.dwLowDateTime);
  // returned "kernelTime" includes "idleTime"
  system = (static_cast<uint64_t>(kernelTime.dwHighDateTime) << 32) + static_cast<uint64_t>(kernelTime.dwLowDateTime) - idle;
  user = (static_cast<uint64_t>(userTime.dwHighDateTime) << 32) + static_cast<uint64_t>(userTime.dwLowDateTime);

  if (m_cpuFreqCounter && PdhCollectQueryData(m_cpuQueryLoad) == ERROR_SUCCESS)
  {
    for (auto& core : m_cores)
    {
      PDH_RAW_COUNTER cnt;
      DWORD cntType;
      if (core.m_coreCounter && PdhGetRawCounterValue(core.m_coreCounter, &cntType, &cnt) == ERROR_SUCCESS &&
          (cnt.CStatus == PDH_CSTATUS_VALID_DATA || cnt.CStatus == PDH_CSTATUS_NEW_DATA))
      {
        const LONGLONG coreTotal = cnt.SecondValue,
                       coreIdle  = cnt.FirstValue;
        const LONGLONG deltaTotal = coreTotal - core.m_totalTime,
                       deltaIdle  = coreIdle - core.m_idleTime;
        const double load = (double(deltaTotal - deltaIdle) * 100.0) / double(deltaTotal);

        // win32 has some problems with calculation of load if load close to zero
        core.m_usagePercent = (load < 0) ? 0 : load;
        if (load >= 0 || deltaTotal > 5 * 10 * 1000 * 1000) // do not update (smooth) values for 5 seconds on negative loads
        {
          core.m_totalTime = coreTotal;
          core.m_idleTime = coreIdle;
        }
      }
      else
        core.m_usagePercent = double(m_lastUsedPercentage); // use CPU average as fallback
    }
  }
  else
    for (auto& core : m_cores)
      core.m_usagePercent = double(m_lastUsedPercentage); // use CPU average as fallback

  m_nextUsedReadTime.Set(MINIMUM_TIME_BETWEEN_READS);

  return result;
}

float CCPUInfoWin32::GetCPUFrequency()
{
  // Get CPU frequency, scaled to MHz.
  if (m_cpuFreqCounter && PdhCollectQueryData(m_cpuQueryFreq) == ERROR_SUCCESS)
  {
    PDH_RAW_COUNTER cnt;
    DWORD cntType;
    if (PdhGetRawCounterValue(m_cpuFreqCounter, &cntType, &cnt) == ERROR_SUCCESS &&
        (cnt.CStatus == PDH_CSTATUS_VALID_DATA || cnt.CStatus == PDH_CSTATUS_NEW_DATA))
    {
      return float(cnt.FirstValue);
    }
  }

  return 0;
}

bool CCPUInfoWin32::GetTemperature(CTemperature& temperature)
{
  return false;
}
