/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <stdint.h>

class BitstreamStats final
{
public:
  // in order not to cause a performance hit, we should only check the clock when
  // we reach m_estimatedBitrate bits.
  // if this value is 1, we will calculate bitrate on every sample.
  explicit BitstreamStats(unsigned int estimatedBitrate = (10240 * 8) /*10Kbit*/);

  void AddSampleBytes(unsigned int bytes);
  void AddSampleBits(unsigned int bits);

  inline double GetBitrate() const { return m_bitrate; }
  inline double GetMaxBitrate() const { return m_maxBitrate; }
  inline double GetMinBitrate() const { return m_minBitrate; }

  void Start();
  void CalculateBitrate();

private:
  double m_bitrate;
  double m_maxBitrate;
  double m_minBitrate;
  unsigned int m_bitCount;
  unsigned int m_estimatedBitrate; // when we reach this amount of bits we check current bitrate.
  int64_t m_tmStart;
  static int64_t m_tmFreq;
};

