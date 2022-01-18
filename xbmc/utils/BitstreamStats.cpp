/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "BitstreamStats.h"

using namespace std::chrono_literals;

BitstreamStats::BitstreamStats(unsigned int estimatedBitrate) : m_estimatedBitrate(estimatedBitrate)
{
}

void BitstreamStats::AddSampleBytes(unsigned int bytes)
{
  AddSampleBits(bytes * 8);
}

void BitstreamStats::AddSampleBits(unsigned int bits)
{
  m_bitCount += bits;
  if (m_bitCount >= m_estimatedBitrate)
    CalculateBitrate();
}

void BitstreamStats::Start()
{
  m_bitCount = 0;
  m_start = std::chrono::steady_clock::now();
}

void BitstreamStats::CalculateBitrate()
{
  auto now = std::chrono::steady_clock::now();

  auto elapsed = now - m_start;

  // only update after 2 seconds has past
  if (elapsed >= 2s)
  {
    m_bitrate = static_cast<double>(m_bitCount) /
                std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();

    if (m_bitrate > m_maxBitrate)
      m_maxBitrate = m_bitrate;

    if (m_bitrate < m_minBitrate || m_minBitrate == -1)
      m_minBitrate = m_bitrate;

    Start();
  }
}




