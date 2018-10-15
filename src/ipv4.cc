// Copyright 2018 Alex Horn. All rights reserved.
// Use of this source code is governed by a LICENSE.

#include "ipv4.hh"

namespace nopticon {

std::ostream &operator<<(std::ostream &ostream, const ip_prefix_t &ip_prefix) {
  static char delim[] = {'/', '.', '.', '.'};
  for (signed char i = 3; i != -1; --i) {
    ostream << static_cast<unsigned>(static_cast<uint8_t>(ip_prefix.ip_addr >>
                                                          (i * __CHAR_BIT__)))
            << delim[i];
  }
  return ostream << ip_prefix_length(ip_prefix);
}

} // namespace nopticon
