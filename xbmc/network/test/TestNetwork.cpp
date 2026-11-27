/*
 *  Copyright (C) 2023 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "ServiceBroker.h"
#include "network/Network.h"
#include "network/NetworkUtils.h"

#include <atomic>
#include <future>
#include <initializer_list>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <netinet/in.h>

#if (defined(TARGET_LINUX) && !defined(TARGET_ANDROID)) || defined(TARGET_FREEBSD) || \
    defined(TARGET_DARWIN_OSX)
#define HAVE_NETWORK_POSIX
#include "platform/posix/network/NetworkPosix.h"

#include <net/if.h>
#endif

using namespace KODI;
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

namespace
{
class CFakeInterface : public CNetworkInterface
{
public:
  CFakeInterface(bool connected, std::string ip, std::string gateway)
    : m_connected(connected),
      m_ip(std::move(ip)),
      m_gateway(std::move(gateway))
  {
  }

  bool IsEnabled() const override { return true; }
  bool IsConnected() const override { return m_connected; }
  std::string GetMacAddress() const override { return "mac-" + m_ip; }
  void GetMacAddressRaw(char[6]) const override {}
  bool GetHostMacAddress(unsigned long, std::string&) const override { return false; }
  std::string GetCurrentIPAddress() const override { return m_ip; }
  std::string GetCurrentNetmask() const override { return "255.255.255.0"; }
  std::string GetCurrentDefaultGateway() const override { return m_gateway; }

private:
  bool m_connected;
  std::string m_ip;
  std::string m_gateway;
};

class CBlockingInterface : public CFakeInterface
{
public:
  CBlockingInterface(std::promise<void>& entered,
                     std::shared_future<void> release,
                     std::atomic<bool>& destroyed)
    : CFakeInterface(true, "192.0.2.1", "192.0.2.254"),
      m_entered(entered),
      m_release(std::move(release)),
      m_destroyed(destroyed)
  {
  }
  ~CBlockingInterface() override { m_destroyed = true; }

  std::string GetHostName() const override
  {
    m_entered.set_value();
    m_release.wait();
    return "blocked-host";
  }

private:
  std::promise<void>& m_entered;
  std::shared_future<void> m_release;
  std::atomic<bool>& m_destroyed;
};

CNetworkInterfaces::InterfaceList MakeInterfaces(
    std::initializer_list<std::tuple<bool, std::string, std::string>> specs)
{
  CNetworkInterfaces::InterfaceList list;
  for (const auto& [connected, ip, gateway] : specs)
    list.push_back(std::make_unique<CFakeInterface>(connected, ip, gateway));
  return list;
}
} // namespace

TEST(TestNetworkInterfaces, PrefersConnectedInterfaceWithGateway)
{
  CNetworkInterfaces interfaces;
  interfaces.SetInterfaces(MakeInterfaces({{false, "192.0.2.1", "192.0.2.254"},
                                           {true, "198.51.100.1", ""},
                                           {true, "203.0.113.1", "203.0.113.254"}}));

  EXPECT_TRUE(interfaces.IsConnected());
  EXPECT_EQ(interfaces.GetCurrentIPAddress(), "203.0.113.1");
  EXPECT_EQ(interfaces.GetDefaultGateway(), "203.0.113.254");
  EXPECT_EQ(interfaces.GetMacAddress(), "mac-203.0.113.1");
}

TEST(TestNetworkInterfaces, FallsBackToConnectedInterfaceWithoutGateway)
{
  CNetworkInterfaces interfaces;
  interfaces.SetInterfaces(
      MakeInterfaces({{false, "192.0.2.1", "192.0.2.254"}, {true, "198.51.100.1", ""}}));

  EXPECT_TRUE(interfaces.IsConnected());
  EXPECT_EQ(interfaces.GetCurrentIPAddress(), "198.51.100.1");
}

TEST(TestNetworkInterfaces, NothingConnected)
{
  CNetworkInterfaces interfaces;
  EXPECT_FALSE(interfaces.IsAvailable());

  interfaces.SetInterfaces(MakeInterfaces({{false, "192.0.2.1", "192.0.2.254"}}));

  EXPECT_TRUE(interfaces.IsAvailable());
  EXPECT_FALSE(interfaces.IsConnected());
  EXPECT_EQ(interfaces.GetCurrentIPAddress(), "");
  char mac[6]{};
  EXPECT_FALSE(interfaces.GetMacAddressRaw(mac));
}

TEST(TestNetworkInterfaces, MatchesSubnetOfAnyConnectedInterface)
{
  CNetworkInterfaces interfaces;
  interfaces.SetInterfaces(MakeInterfaces({{true, "192.0.2.1", "192.0.2.254"},
                                           {true, "198.51.100.1", ""},
                                           {false, "203.0.113.1", ""}}));

  EXPECT_TRUE(interfaces.HasInterfaceForIP(ntohl(inet_addr("192.0.2.77"))));
  EXPECT_TRUE(interfaces.HasInterfaceForIP(ntohl(inet_addr("198.51.100.77"))));
  EXPECT_FALSE(interfaces.HasInterfaceForIP(ntohl(inet_addr("203.0.113.77"))));
  EXPECT_TRUE(interfaces.HasIPAddress("203.0.113.1"));
  EXPECT_FALSE(interfaces.HasIPAddress("203.0.113.2"));
}

TEST(TestNetworkInterfaces, DefaultInterfaceWins)
{
  CNetworkInterfaces interfaces;
  interfaces.SetInterfaces(MakeInterfaces({{true, "192.0.2.1", "192.0.2.254"}}));
  interfaces.SetDefaultInterface(std::make_unique<CFakeInterface>(true, "198.51.100.1", ""));

  EXPECT_EQ(interfaces.GetCurrentIPAddress(), "198.51.100.1");

  interfaces.SetDefaultInterface(nullptr);

  EXPECT_EQ(interfaces.GetCurrentIPAddress(), "192.0.2.1");
}

TEST(TestNetworkInterfaces, BlockedLookupDoesNotHoldUpOthers)
{
  std::promise<void> entered;
  std::promise<void> release;
  std::atomic<bool> destroyed{false};

  CNetworkInterfaces interfaces;
  CNetworkInterfaces::InterfaceList list;
  list.push_back(
      std::make_unique<CBlockingInterface>(entered, release.get_future().share(), destroyed));
  interfaces.SetInterfaces(std::move(list));

  auto hostName = std::async(std::launch::async, [&] { return interfaces.GetHostName(); });
  entered.get_future().wait();

  auto requery = std::async(std::launch::async,
                            [&]
                            {
                              interfaces.SetInterfaces(
                                  MakeInterfaces({{true, "198.51.100.1", "198.51.100.254"}}));
                              return interfaces.GetCurrentIPAddress();
                            });
  const bool requeried = requery.wait_for(5s) == std::future_status::ready;
  EXPECT_FALSE(destroyed);

  release.set_value();
  ASSERT_TRUE(requeried);
  EXPECT_EQ(requery.get(), "198.51.100.1");
  EXPECT_EQ(hostName.get(), "blocked-host");
  EXPECT_TRUE(destroyed);
}

TEST(TestNetworkUtils, GetIpStr)
{
  sockaddr_in ipv4{};
  ipv4.sin_family = AF_INET;
  ASSERT_EQ(inet_pton(AF_INET, "192.0.2.1", &ipv4.sin_addr), 1);
  EXPECT_EQ(NETWORK::GetIpStr(reinterpret_cast<const sockaddr*>(&ipv4)), "192.0.2.1");

  sockaddr_in6 ipv6{};
  ipv6.sin6_family = AF_INET6;
  ASSERT_EQ(inet_pton(AF_INET6, "2001:db8::1", &ipv6.sin6_addr), 1);
  EXPECT_EQ(NETWORK::GetIpStr(reinterpret_cast<const sockaddr*>(&ipv6)), "2001:db8::1");

  sockaddr unknown{};
  unknown.sa_family = AF_UNSPEC;
  EXPECT_EQ(NETWORK::GetIpStr(&unknown), "");
  EXPECT_EQ(NETWORK::GetIpStr(nullptr), "");
}

#ifdef HAVE_NETWORK_POSIX
TEST(TestNetworkUtils, SetInterfaceName)
{
  ifreq request{};
  const std::string longest(sizeof(request.ifr_name) - 1, 'x');
  ASSERT_TRUE(NETWORK::SetInterfaceName(request, longest));
  EXPECT_EQ(std::string(request.ifr_name), longest);

  EXPECT_FALSE(NETWORK::SetInterfaceName(request, longest + "x"));
}
#endif

TEST(TestNetworkUtils, GetIpStrInAddr)
{
  in_addr address{};
  ASSERT_EQ(inet_pton(AF_INET, "198.51.100.254", &address), 1);
  EXPECT_EQ(NETWORK::GetIpStr(address), "198.51.100.254");

  address.s_addr = htonl(INADDR_ANY);
  EXPECT_EQ(NETWORK::GetIpStr(address), "0.0.0.0");
}

TEST(TestNetworkUtils, GetMaskByPrefixLength)
{
  EXPECT_EQ(NETWORK::GetMaskByPrefixLength(0), "0.0.0.0");
  EXPECT_EQ(NETWORK::GetMaskByPrefixLength(1), "128.0.0.0");
  EXPECT_EQ(NETWORK::GetMaskByPrefixLength(8), "255.0.0.0");
  EXPECT_EQ(NETWORK::GetMaskByPrefixLength(23), "255.255.254.0");
  EXPECT_EQ(NETWORK::GetMaskByPrefixLength(24), "255.255.255.0");
  EXPECT_EQ(NETWORK::GetMaskByPrefixLength(32), "255.255.255.255");
  EXPECT_EQ(NETWORK::GetMaskByPrefixLength(33), "");
}

namespace
{
class CLocalNetwork : public CNetworkBase
{
public:
  CLocalNetwork()
  {
    GetInterfaces().SetInterfaces(MakeInterfaces({{true, "192.0.2.1", "192.0.2.254"}}));
  }

  bool GetHostName(std::string& hostname) override
  {
    hostname = "kodi-box";
    return true;
  }
  bool IcmpPing(unsigned long, std::chrono::milliseconds) override { return false; }
  std::vector<std::string> GetNameServers() override { return {}; }
};
} // namespace

TEST(TestNetworkBase, IsLocalHost)
{
  CLocalNetwork network;
  EXPECT_FALSE(network.IsLocalHost(""));
  EXPECT_TRUE(network.IsLocalHost("127.0.0.1"));
  EXPECT_TRUE(network.IsLocalHost("127.1.2.3"));
  EXPECT_TRUE(network.IsLocalHost("::1"));
  EXPECT_TRUE(network.IsLocalHost("LocalHost"));
  EXPECT_TRUE(network.IsLocalHost("Kodi-Box"));
  EXPECT_TRUE(network.IsLocalHost("192.0.2.1"));
  EXPECT_FALSE(network.IsLocalHost("192.0.2.2"));
  EXPECT_FALSE(network.IsLocalHost("example.com"));
}
