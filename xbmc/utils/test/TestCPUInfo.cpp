/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#if defined(TARGET_WINDOWS)
#  include <windows.h>
#endif

#include "utils/CPUInfo.h"
#include "utils/Temperature.h"
#include "ServiceBroker.h"
#include "settings/AdvancedSettings.h"
#include "settings/SettingsComponent.h"

#ifdef TARGET_POSIX
#include "platform/posix/XTimeUtils.h"
#endif

#include "gtest/gtest.h"

struct TestCPUInfo : public ::testing::Test
{
  TestCPUInfo()
  {
    CServiceBroker::RegisterCPUInfo(CCPUInfo::GetCPUInfo());
  }

  ~TestCPUInfo()
  {
    CServiceBroker::UnregisterCPUInfo();
  }
};

TEST_F(TestCPUInfo, GetUsedPercentage)
{
  EXPECT_GE(CServiceBroker::GetCPUInfo()->GetUsedPercentage(), 0);
}

TEST_F(TestCPUInfo, GetCPUCount)
{
  EXPECT_GT(CServiceBroker::GetCPUInfo()->GetCPUCount(), 0);
}

TEST_F(TestCPUInfo, GetCPUFrequency)
{
  EXPECT_GE(CServiceBroker::GetCPUInfo()->GetCPUFrequency(), 0.f);
}

namespace
{
class TemporarySetting
{
public:

  TemporarySetting(std::string &setting, const char *newValue) :
    m_Setting(setting),
    m_OldValue(setting)
  {
    m_Setting = newValue;
  }

  ~TemporarySetting()
  {
    m_Setting = m_OldValue;
  }

private:

  std::string &m_Setting;
  std::string m_OldValue;
};
}

//Disabled for windows because there is no implementation to get the CPU temp and there will probably never be one
#ifndef TARGET_WINDOWS
TEST_F(TestCPUInfo, GetTemperature)
{
  TemporarySetting command(CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_cpuTempCmd, "echo '50 c'");
  CTemperature t;
  EXPECT_TRUE(CServiceBroker::GetCPUInfo()->GetTemperature(t));
  EXPECT_TRUE(t.IsValid());
}
#endif

TEST_F(TestCPUInfo, GetCPUModel)
{
  std::string s = CServiceBroker::GetCPUInfo()->GetCPUModel();
  EXPECT_STRNE("", s.c_str());
}

TEST_F(TestCPUInfo, GetCPUBogoMips)
{
  std::string s = CServiceBroker::GetCPUInfo()->GetCPUBogoMips();
  EXPECT_STRNE("", s.c_str());
}

TEST_F(TestCPUInfo, GetCPUHardware)
{
  std::string s = CServiceBroker::GetCPUInfo()->GetCPUHardware();
  EXPECT_STRNE("", s.c_str());
}

TEST_F(TestCPUInfo, GetCPURevision)
{
  std::string s = CServiceBroker::GetCPUInfo()->GetCPURevision();
  EXPECT_STRNE("", s.c_str());
}

TEST_F(TestCPUInfo, GetCPUSerial)
{
  std::string s = CServiceBroker::GetCPUInfo()->GetCPUSerial();
  EXPECT_STRNE("", s.c_str());
}

TEST_F(TestCPUInfo, CoreInfo)
{
  ASSERT_TRUE(CServiceBroker::GetCPUInfo()->HasCoreId(0));
  const CoreInfo c = CServiceBroker::GetCPUInfo()->GetCoreInfo(0);
  EXPECT_TRUE(c.m_id == 0);
}

TEST_F(TestCPUInfo, GetCoresUsageString)
{
  EXPECT_STRNE("", CServiceBroker::GetCPUInfo()->GetCoresUsageString().c_str());
}

TEST_F(TestCPUInfo, GetCPUFeatures)
{
  unsigned int a = CServiceBroker::GetCPUInfo()->GetCPUFeatures();
  (void)a;
}

TEST_F(TestCPUInfo, GetUsedPercentage_output)
{
  Sleep(1); //! @todo Support option from main that sets this parameter
  int r = CServiceBroker::GetCPUInfo()->GetUsedPercentage();
  std::cout << "Percentage: " << testing::PrintToString(r) << std::endl;
}
