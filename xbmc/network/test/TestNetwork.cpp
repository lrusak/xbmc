/*
 *  Copyright (C) 2023 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "ServiceBroker.h"
#include "network/Network.h"

#include <arpa/inet.h>
#include <gtest/gtest.h>

using namespace std::chrono_literals;

class TestNetwork : public testing::Test
{
public:
  TestNetwork() = default;
  ~TestNetwork() = default;

  bool PingHost(const std::string& ip) const
  {
    static auto& network = CServiceBroker::GetNetwork();

    return network.IcmpPing(inet_addr(ip.c_str()), GetTimeout());
  }

  std::chrono::milliseconds GetTimeout() const { return m_timeout; }

private:
  std::chrono::milliseconds m_timeout{100ms};
};

TEST_F(TestNetwork, PingHost)
{
  EXPECT_TRUE(PingHost("127.0.0.1"));
  EXPECT_FALSE(PingHost("10.254.254.254"));
}
