/*
 *  Copyright (C) 2005-2021 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

template<typename FlagBitsType>
struct FlagTraits
{
  enum
  {
    allFlags = 0
  };
};

template<typename BitType>
class Flags
{
public:
  using MaskType = typename std::underlying_type<BitType>::type;

  // constructors
  constexpr Flags() : m_mask(0) {}

  constexpr Flags(BitType bit) : m_mask(static_cast<MaskType>(bit)) {}

  constexpr Flags(Flags<BitType> const& rhs) = default;

  constexpr explicit Flags(MaskType flags) : m_mask(flags) {}

  // relational operators
  constexpr bool operator<(Flags<BitType> const& rhs) const { return m_mask < rhs.m_mask; }

  constexpr bool operator<=(Flags<BitType> const& rhs) const { return m_mask <= rhs.m_mask; }

  constexpr bool operator>(Flags<BitType> const& rhs) const { return m_mask > rhs.m_mask; }

  constexpr bool operator>=(Flags<BitType> const& rhs) const { return m_mask >= rhs.m_mask; }

  constexpr bool operator==(Flags<BitType> const& rhs) const { return m_mask == rhs.m_mask; }

  constexpr bool operator!=(Flags<BitType> const& rhs) const { return m_mask != rhs.m_mask; }

  // logical operator
  constexpr bool operator!() const { return !m_mask; }

  // bitwise operators
  constexpr Flags<BitType> operator&(Flags<BitType> const& rhs) const
  {
    return Flags<BitType>(m_mask & rhs.m_mask);
  }

  constexpr Flags<BitType> operator|(Flags<BitType> const& rhs) const
  {
    return Flags<BitType>(m_mask | rhs.m_mask);
  }

  constexpr Flags<BitType> operator^(Flags<BitType> const& rhs) const
  {
    return Flags<BitType>(m_mask ^ rhs.m_mask);
  }

  constexpr Flags<BitType> operator~() const
  {
    return Flags<BitType>(m_mask ^ FlagTraits<BitType>::allFlags);
  }

  // assignment operators
  constexpr Flags<BitType>& operator=(Flags<BitType> const& rhs) = default;

  constexpr Flags<BitType>& operator|=(Flags<BitType> const& rhs)
  {
    m_mask |= rhs.m_mask;
    return *this;
  }

  constexpr Flags<BitType>& operator&=(Flags<BitType> const& rhs)
  {
    m_mask &= rhs.m_mask;
    return *this;
  }

  constexpr Flags<BitType>& operator^=(Flags<BitType> const& rhs)
  {
    m_mask ^= rhs.m_mask;
    return *this;
  }

  // cast operators
  explicit constexpr operator bool() const { return !!m_mask; }

  explicit constexpr operator MaskType() const { return m_mask; }

private:
  MaskType m_mask;
};

// relational operators only needed for pre C++20
template<typename BitType>
constexpr bool operator<(BitType bit, Flags<BitType> const& flags)
{
  return flags.operator>(bit);
}

template<typename BitType>
constexpr bool operator<=(BitType bit, Flags<BitType> const& flags)
{
  return flags.operator>=(bit);
}

template<typename BitType>
constexpr bool operator>(BitType bit, Flags<BitType> const& flags)
{
  return flags.operator<(bit);
}

template<typename BitType>
constexpr bool operator>=(BitType bit, Flags<BitType> const& flags)
{
  return flags.operator<=(bit);
}

template<typename BitType>
constexpr bool operator==(BitType bit, Flags<BitType> const& flags)
{
  return flags.operator==(bit);
}

template<typename BitType>
constexpr bool operator!=(BitType bit, Flags<BitType> const& flags)
{
  return flags.operator!=(bit);
}

// bitwise operators
template<typename BitType>
constexpr Flags<BitType> operator&(BitType bit, Flags<BitType> const& flags)
{
  return flags.operator&(bit);
}

template<typename BitType>
constexpr Flags<BitType> operator|(BitType bit, Flags<BitType> const& flags)
{
  return flags.operator|(bit);
}

template<typename BitType>
constexpr Flags<BitType> operator^(BitType bit, Flags<BitType> const& flags)
{
  return flags.operator^(bit);
}
