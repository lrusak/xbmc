/*
 *  Copyright (C) 2005-2022 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "threads/Thread.h"

#include <memory>
#include <string>
#include <thread>

class IThreadImpl
{
public:
  virtual ~IThreadImpl() = default;

  static std::unique_ptr<IThreadImpl> CreateThreadImpl(std::thread::native_handle_type handle,
                                                       const std::string& name);

  virtual bool SetPriority(const ThreadPriority priority) = 0;

protected:
  explicit IThreadImpl(std::thread::native_handle_type handle, const std::string& name)
    : m_handle(handle), m_name(name)
  {
  }

  std::thread::native_handle_type m_handle;
  std::string m_name;

private:
  IThreadImpl() = delete;
};
