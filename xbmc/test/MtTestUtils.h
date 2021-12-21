/*
 *  Copyright (C) 2005-2019 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "threads/SystemClock.h"

#include <chrono>
#include <thread>

using namespace std::chrono_literals;

namespace ConditionPoll
{
/**
 * This is usually enough time for the condition to have occurred in a test.
 */
constexpr auto defaultTimeout{20000ms};

/**
 * poll until the lambda returns true or the timeout occurs. If the timeout occurs then
 * the function will return false. Otherwise it will return true.
 */
template<typename L>
inline bool poll(std::chrono::milliseconds timeoutMillis, L lambda)
{
  XbmcThreads::EndTime<> endTime{timeoutMillis};
  bool lastValue = false;
  while (!endTime.IsTimePast() && (lastValue = lambda()) == false)
    std::this_thread::sleep_for(50ms);
  return lastValue;
}

/**
 * poll until the lambda returns true or the defaultTimeout occurs. If the timeout occurs then
 * the function will return false. Otherwise it will return true.
 */
template<typename L> inline bool poll(L lambda)
{
  return poll(defaultTimeout, lambda);
}

}
