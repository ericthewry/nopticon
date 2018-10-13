// Copyright 2018 Alex Horn. All rights reserved.
// Use of this source code is governed by a LICENSE.

#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <map>
#include <vector>

namespace nopticon {

typedef uint32_t ip_addr_t;
typedef std::vector<ip_addr_t> ip_addr_vec_t;

struct ip_prefix_t {
  static constexpr auto MAX_LEN = sizeof(ip_addr_t) * __CHAR_BIT__;
  static_assert(MAX_LEN <= std::numeric_limits<uint8_t>::max(),
                "Type too narrow");

  ip_addr_t ip_addr, mask;

  ip_prefix_t() noexcept
      : ip_addr{0}, mask{std::numeric_limits<ip_addr_t>::max()} {}

  ip_prefix_t(ip_addr_t ip_addr, uint8_t len)
      : ip_addr{ip_addr}, mask{(ip_addr_t{1} << (MAX_LEN - len)) -
                               ip_addr_t{1}} {
    assert(0 < len);
    assert(len <= MAX_LEN);
  }

  ip_prefix_t &operator=(const ip_prefix_t &) noexcept = default;
  bool is_valid() const noexcept { return ip_addr == (ip_addr & ~mask); }
};

/// Inclusive range of IP addresses
struct ip_range_t {
  ip_range_t() : low{0}, high{std::numeric_limits<ip_addr_t>::max()} {}

  ip_range_t(const ip_prefix_t &ip_prefix)
      : low{ip_prefix.ip_addr}, high{low + ip_prefix.mask} {}

  ip_range_t(ip_addr_t low, ip_addr_t high) : low{low}, high{high} {
    assert(low <= high);
  }

  ip_addr_t low, high;
};

typedef std::vector<ip_range_t> ip_range_vec_t;

inline bool operator==(const ip_prefix_t &x, const ip_prefix_t &y) noexcept {
  return x.ip_addr == y.ip_addr and x.mask == y.mask;
}

inline bool operator!=(const ip_prefix_t &x, const ip_prefix_t &y) noexcept {
  return not(x == y);
}

inline bool overlaps(const ip_prefix_t &x, const ip_prefix_t &y) noexcept {
  return (x.ip_addr ^ y.ip_addr) <= (x.mask | y.mask);
}

inline bool subset(const ip_prefix_t &x, const ip_prefix_t &y) noexcept {
  return (x.ip_addr ^ y.ip_addr) <= y.mask and x.mask <= y.mask;
}

inline bool operator==(const ip_range_t &x, const ip_range_t &y) noexcept {
  return x.low == y.low and x.high == y.high;
}

inline bool operator!=(const ip_range_t &x, const ip_range_t &y) noexcept {
  return not(x == y);
}

inline bool overlaps(const ip_range_t &x, const ip_range_t &y) {
  return not(x.high < y.low or y.high < x.low);
}

inline bool subset(const ip_range_t &x, const ip_range_t &y) {
  return y.low <= x.low and x.high <= y.high;
}

inline unsigned ip_prefix_length(const ip_prefix_t &x) noexcept {
  auto len = ip_prefix_t::MAX_LEN;
  for (ip_addr_t mask = 1UL; len != 0; mask <<= 1, --len) {
    if (x.mask == (mask - 1)) {
      return len;
    }
  }
  return 0;
}

struct ip_prefix_order_t {
  inline bool operator()(const ip_prefix_t &x, const ip_prefix_t &y) const
      noexcept {
    if (x.ip_addr == y.ip_addr) {
      return x.mask > y.mask;
    }
    return x.ip_addr < y.ip_addr;
  }
};

template <class T>
using ip_prefix_map_t = std::map<ip_prefix_t, T, ip_prefix_order_t>;

} // namespace nopticon
