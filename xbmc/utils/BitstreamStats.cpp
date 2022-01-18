/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "BitstreamStats.h"

#include "utils/TimeUtils.h"

int64_t BitstreamStats::m_tmFreq;

BitstreamStats::BitstreamStats(unsigned int estimatedBitrate)
{
  m_bitrate = 0.0;
  m_maxBitrate = 0.0;
  m_minBitrate = -1.0;

  m_bitCount = 0;
  m_estimatedBitrate = estimatedBitrate;
  m_tmStart = 0LL;

  if (m_tmFreq == 0LL)
    m_tmFreq = CurrentHostFrequency();
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
  m_tmStart = CurrentHostCounter();
}

void BitstreamStats::CalculateBitrate()
{
  int64_t tmNow;
  tmNow = CurrentHostCounter();

  double elapsed = (double)(tmNow - m_tmStart) / (double)m_tmFreq;
  // only update once every 2 seconds
  if (elapsed >= 2)
  {
    m_bitrate = (double)m_bitCount / elapsed;

    if (m_bitrate > m_maxBitrate)
      m_maxBitrate = m_bitrate;

    if (m_bitrate < m_minBitrate || m_minBitrate == -1)
      m_minBitrate = m_bitrate;

    Start();
  }
}




