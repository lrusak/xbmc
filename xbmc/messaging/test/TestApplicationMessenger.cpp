/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "messaging/ApplicationMessenger.h"
#include "messaging/IMessageTarget.h"
#include "messaging/ThreadMessage.h"

#include <cstdint>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

using namespace KODI::MESSAGING;

namespace
{
class CRecordingTarget : public IMessageTarget
{
public:
  explicit CRecordingTarget(int mask) : m_mask(mask) {}

  int GetMessageMask() override { return m_mask; }
  void OnApplicationMessage(ThreadMessage* msg) override { messages.push_back(msg->dwMessage); }

  std::vector<uint32_t> messages;

private:
  int m_mask;
};

class TestApplicationMessenger : public ::testing::Test
{
protected:
  TestApplicationMessenger()
  {
    m_messenger.RegisterReceiver(&m_app);
    m_messenger.RegisterReceiver(&m_windows);
  }

  CApplicationMessenger m_messenger;
  CRecordingTarget m_app{TMSG_MASK_APPLICATION};
  CRecordingTarget m_windows{TMSG_MASK_WINDOWMANAGER};
};
} // namespace

TEST_F(TestApplicationMessenger, GUIMessagesAreQueuedForTheGUIThread)
{
  m_messenger.PostMsg(TMSG_QUIT);
  m_messenger.PostMsg(TMSG_GUI_PREVIOUS_WINDOW);
  m_messenger.PostMsg(TMSG_SETVIDEORESOLUTION);

  m_messenger.ProcessWindowMessages();
  EXPECT_TRUE(m_app.messages == std::vector<uint32_t>{TMSG_SETVIDEORESOLUTION});
  EXPECT_TRUE(m_windows.messages == std::vector<uint32_t>{TMSG_GUI_PREVIOUS_WINDOW});

  m_messenger.ProcessMessages();
  EXPECT_TRUE((m_app.messages == std::vector<uint32_t>{TMSG_SETVIDEORESOLUTION, TMSG_QUIT}));
}

TEST_F(TestApplicationMessenger, SendingFromTheHandlingThreadDoesNotWait)
{
  std::thread other([] {});
  m_messenger.SetProcessThread(other.get_id());
  m_messenger.SetGUIThread(std::this_thread::get_id());

  m_messenger.SendMsg(TMSG_GUI_PREVIOUS_WINDOW);
  EXPECT_TRUE(m_windows.messages == std::vector<uint32_t>{TMSG_GUI_PREVIOUS_WINDOW});

  m_messenger.SetProcessThread(std::this_thread::get_id());
  m_messenger.SetGUIThread(other.get_id());

  m_messenger.SendMsg(TMSG_QUIT);
  EXPECT_TRUE(m_app.messages == std::vector<uint32_t>{TMSG_QUIT});

  other.join();
}
