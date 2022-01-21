/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <chrono>

class BitstreamStats final
{
public:
  BitstreamStats() = default;
  ~BitstreamStats() = default;

  void AddSampleBytes(uint32_t bytes);
  void AddSampleBits(uint32_t bits);

  inline double GetBitrate() const { return m_bitrate; }
  inline double GetMaxBitrate() const { return m_maxBitrate; }
  inline double GetMinBitrate() const { return m_minBitrate; }

  void Start();

  /**
   * @brief Calculates the bitrate if 2 seconds
   *        has passed since the last call.
   *
   */
  void CalculateBitrate();

private:
  double m_bitrate{0.0};
  double m_maxBitrate{0.0};
  double m_minBitrate{-1.0};
  uint32_t m_bitCount{0};
  uint32_t m_estimatedBitrate{1024 * 10 * 8}; // 1KB * 10 * 8bit/byte = 10Kbit
  std::chrono::steady_clock::time_point m_start;
};

