/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "Network.h"

#include "ServiceBroker.h"
#include "messaging/ApplicationMessenger.h"
#include "network/NetworkServices.h"
#include "network/NetworkUtils.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "utils/log.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#ifdef TARGET_WINDOWS
#include "platform/win32/WIN32Util.h"
#include "utils/CharsetConverter.h"
#endif
#include "utils/StringUtils.h"
#include "utils/XTimeUtils.h"

#include <algorithm>

CNetworkBase::CNetworkBase() :
  m_services(new CNetworkServices())
{
  CServiceBroker::GetAppMessenger()->PostMsg(TMSG_NETWORKMESSAGE, SERVICES_UP, 0);
}

CNetworkBase::~CNetworkBase()
{
  CServiceBroker::GetAppMessenger()->PostMsg(TMSG_NETWORKMESSAGE, SERVICES_DOWN, 0);
}

void CNetworkInterfaces::SetInterfaces(InterfaceList interfaces)
{
  auto list = std::make_shared<const InterfaceList>(std::move(interfaces));
  std::unique_lock lock(m_mutex);
  m_state.interfaces.swap(list);
}

void CNetworkInterfaces::SetDefaultInterface(std::unique_ptr<CNetworkInterface> interface)
{
  std::shared_ptr<const CNetworkInterface> defaultInterface = std::move(interface);
  std::unique_lock lock(m_mutex);
  m_state.defaultInterface.swap(defaultInterface);
}

CNetworkInterfaces::State CNetworkInterfaces::GetState() const
{
  std::unique_lock lock(m_mutex);
  return m_state;
}

const CNetworkInterface* CNetworkInterfaces::GetFirstConnectedInterface(const State& state)
{
  if (state.defaultInterface)
    return state.defaultInterface.get();

  const CNetworkInterface* fallbackInterface = nullptr;
  for (const auto& iface : *state.interfaces)
  {
    if (iface->IsConnected())
    {
      if (!iface->GetCurrentDefaultGateway().empty())
        return iface.get();
      else if (fallbackInterface == nullptr)
        fallbackInterface = iface.get();
    }
  }

  return fallbackInterface;
}

bool CNetworkInterfaces::IsAvailable() const
{
  std::unique_lock lock(m_mutex);
  return !m_state.interfaces->empty();
}

bool CNetworkInterfaces::IsConnected() const
{
  return GetFirstConnectedInterface(GetState()) != nullptr;
}

std::string CNetworkInterfaces::GetCurrentIPAddress() const
{
  const State state = GetState();
  const CNetworkInterface* iface = GetFirstConnectedInterface(state);
  return iface ? iface->GetCurrentIPAddress() : std::string{};
}

std::string CNetworkInterfaces::GetMacAddress() const
{
  const State state = GetState();
  const CNetworkInterface* iface = GetFirstConnectedInterface(state);
  return iface ? iface->GetMacAddress() : std::string{};
}

std::optional<KODI::NETWORK::MacAddress> CNetworkInterfaces::GetMacAddressRaw() const
{
  const State state = GetState();
  const CNetworkInterface* iface = GetFirstConnectedInterface(state);
  if (!iface)
    return std::nullopt;

  return iface->GetMacAddressRaw();
}

std::string CNetworkInterfaces::GetNetmask() const
{
  const State state = GetState();
  const CNetworkInterface* iface = GetFirstConnectedInterface(state);
  return iface ? iface->GetCurrentNetmask() : std::string{};
}

std::string CNetworkInterfaces::GetDefaultGateway() const
{
  const State state = GetState();
  const CNetworkInterface* iface = GetFirstConnectedInterface(state);
  return iface ? iface->GetCurrentDefaultGateway() : std::string{};
}

std::string CNetworkInterfaces::GetHostName() const
{
  const State state = GetState();
  const CNetworkInterface* iface = GetFirstConnectedInterface(state);
  return iface ? iface->GetHostName() : std::string{};
}

std::vector<std::string> CNetworkInterfaces::GetNameServers() const
{
  const State state = GetState();
  const CNetworkInterface* iface = GetFirstConnectedInterface(state);
  return iface ? iface->GetNameServers() : std::vector<std::string>{};
}

bool CNetworkInterfaces::HasIPAddress(std::string_view address) const
{
  const State state = GetState();
  return std::ranges::any_of(*state.interfaces, [address](const auto& iface)
                             { return iface->GetCurrentIPAddress() == address; });
}

bool CNetworkInterfaces::HasInterfaceForIP(unsigned long address) const
{
  const State state = GetState();
  return std::ranges::any_of(*state.interfaces,
                             [address](const auto& iface)
                             {
                               if (!iface->IsConnected())
                                 return false;

                               const unsigned long subnet =
                                   ntohl(inet_addr(iface->GetCurrentNetmask().c_str()));
                               const unsigned long local =
                                   ntohl(inet_addr(iface->GetCurrentIPAddress().c_str()));
                               return (address & subnet) == (local & subnet);
                             });
}

bool CNetworkInterfaces::GetHostMacAddress(unsigned long host, std::string& mac) const
{
  const State state = GetState();
  return std::ranges::any_of(*state.interfaces, [host, &mac](const auto& iface)
                             { return iface->GetHostMacAddress(host, mac); });
}

bool CNetworkBase::GetHostName(std::string& hostname)
{
  UpdateInterfaces();
  hostname = GetInterfaces().GetHostName();
  if (!hostname.empty())
    return true;

  char hostName[128];
  if (gethostname(hostName, sizeof(hostName)))
    return false;

#ifdef TARGET_WINDOWS
  std::string hostStr;
  g_charsetConverter.systemToUtf8(hostName, hostStr);
  hostname = hostStr;
#else
  hostname = hostName;
#endif
  return true;
}

bool CNetworkBase::IsLocalHost(const std::string& hostname)
{
  if (hostname.empty())
    return false;

  if (StringUtils::StartsWith(hostname, "127.")
      || (hostname == "::1")
      || StringUtils::EqualsNoCase(hostname, "localhost"))
    return true;

  std::string myhostname;
  if (GetHostName(myhostname)
      && StringUtils::EqualsNoCase(hostname, myhostname))
    return true;

  UpdateInterfaces();
  return GetInterfaces().HasIPAddress(hostname);
}

std::string CNetworkBase::GetCurrentIPAddress()
{
  UpdateInterfaces();
  return GetInterfaces().GetCurrentIPAddress();
}

std::string CNetworkBase::GetMacAddress()
{
  UpdateInterfaces();
  return GetInterfaces().GetMacAddress();
}

std::optional<KODI::NETWORK::MacAddress> CNetworkBase::GetMacAddressRaw()
{
  UpdateInterfaces();
  return GetInterfaces().GetMacAddressRaw();
}

std::string CNetworkBase::GetNetmask()
{
  UpdateInterfaces();
  return GetInterfaces().GetNetmask();
}

std::string CNetworkBase::GetDefaultGateway()
{
  UpdateInterfaces();
  return GetInterfaces().GetDefaultGateway();
}

bool CNetworkBase::GetHostMacAddress(unsigned long host, std::string& mac)
{
  UpdateInterfaces();
  return GetInterfaces().GetHostMacAddress(host, mac);
}

bool CNetworkBase::HasInterfaceForIP(unsigned long address)
{
  UpdateInterfaces();
  return GetInterfaces().HasInterfaceForIP(address);
}

bool CNetworkBase::IsAvailable(void)
{
  UpdateInterfaces();
  return GetInterfaces().IsAvailable();
}

bool CNetworkBase::IsConnected()
{
  UpdateInterfaces();
  return GetInterfaces().IsConnected();
}

void CNetworkBase::NetworkMessage(EMESSAGE message, int param)
{
  switch( message )
  {
    case SERVICES_UP:
      CLog::Log(LOGDEBUG, "{} - Starting network services", __FUNCTION__);
      m_services->Start();
      break;

    case SERVICES_DOWN:
      CLog::Log(LOGDEBUG, "{} - Signaling network services to stop", __FUNCTION__);
      m_services->Stop(false); // tell network services to stop, but don't wait for them yet
      CLog::Log(LOGDEBUG, "{} - Waiting for network services to stop", __FUNCTION__);
      m_services->Stop(true); // wait for network services to stop
      break;
  }
}

// ping helper
static const char* ConnectHostPort(SOCKET soc, const struct sockaddr_in& addr, struct timeval& timeOut, bool tryRead)
{
  // set non-blocking
#ifdef TARGET_WINDOWS
  u_long nonblocking = 1;
  int result = ioctlsocket(soc, FIONBIO, &nonblocking);
#else
  int result = fcntl(soc, F_SETFL, fcntl(soc, F_GETFL) | O_NONBLOCK);
#endif

  if (result != 0)
    return "set non-blocking option failed";

  result = connect(soc, (const struct sockaddr *)&addr, sizeof(addr)); // non-blocking connect, will fail ..

  if (result < 0)
  {
#ifdef TARGET_WINDOWS
    if (WSAGetLastError() != WSAEWOULDBLOCK)
#else
    if (errno != EINPROGRESS)
#endif
      return "unexpected connect fail";

    { // wait for connect to complete
      fd_set wset;
      FD_ZERO(&wset);
      FD_SET(soc, &wset);

      result = select(FD_SETSIZE, 0, &wset, 0, &timeOut);
    }

    if (result < 0)
      return "select fail";

    if (result == 0) // timeout
      return ""; // no error

    { // verify socket connection state
      int err_code = -1;
      socklen_t code_len = sizeof (err_code);

      result = getsockopt(soc, SOL_SOCKET, SO_ERROR, (char*) &err_code, &code_len);

      if (result != 0)
        return "getsockopt fail";

      if (err_code != 0)
        return ""; // no error, just not connected
    }
  }

  if (tryRead)
  {
    fd_set rset;
    FD_ZERO(&rset);
    FD_SET(soc, &rset);

    result = select(FD_SETSIZE, &rset, 0, 0, &timeOut);

    if (result > 0)
    {
      char message [32];

      result = recv(soc, message, sizeof(message), 0);
    }

    if (result == 0)
      return ""; // no reply yet

    if (result < 0)
      return "recv fail";
  }

  return 0; // success
}

bool CNetworkBase::PingHost(unsigned long ipaddr, unsigned short port, unsigned int timeOutMs, bool readability_check)
{
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = ipaddr;

  SOCKET soc = socket(AF_INET, SOCK_STREAM, 0);

  const char* err_msg = "invalid socket";

  if (soc != INVALID_SOCKET)
  {
    struct timeval tmout;
    tmout.tv_sec = timeOutMs / 1000;
    tmout.tv_usec = (timeOutMs % 1000) * 1000;

    err_msg = ConnectHostPort (soc, addr, tmout, readability_check);

    (void) closesocket (soc);
  }

  if (err_msg && *err_msg)
  {
#ifdef TARGET_WINDOWS
    std::string sock_err = CWIN32Util::WUSysMsg(WSAGetLastError());
#else
    std::string sock_err = strerror(errno);
#endif

    CLog::Log(LOGERROR, "{}({}:{}) - {} ({})", __FUNCTION__, inet_ntoa(addr.sin_addr), port,
              err_msg, sock_err);
  }

  return err_msg == 0;
}

void CNetworkBase::WaitForNet()
{
  const int timeout = CServiceBroker::GetSettingsComponent()->GetSettings()->GetInt(CSettings::SETTING_POWERMANAGEMENT_WAITFORNETWORK);
  if (timeout <= 0)
    return; // wait for network is disabled

  // check if we have at least one network interface to wait for
  if (!IsAvailable())
    return;

  CLog::Log(LOGINFO, "{}: Waiting for a network interface to come up (Timeout: {} s)", __FUNCTION__,
            timeout);

  const static int intervalMs = 200;
  const int numMaxTries = (timeout * 1000) / intervalMs;

  for(int i=0; i < numMaxTries; ++i)
  {
    if (i > 0)
      KODI::TIME::Sleep(std::chrono::milliseconds(intervalMs));

    if (IsConnected())
    {
      CLog::Log(LOGINFO, "{}: A network interface is up after waiting {} ms", __FUNCTION__,
                i * intervalMs);
      return;
    }
  }

  CLog::Log(LOGINFO, "{}: No network interface did come up within {} s... Giving up...",
            __FUNCTION__, timeout);
}
