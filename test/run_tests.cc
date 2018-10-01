// Copyright 2018 Alex Horn. All rights reserved.
// Use of this source code is governed by a LICENSE.

#undef NDEBUG

#include "flow_graph_test.hh"
#include "ipv4_test.hh"

int main() {
  run_ipv4_test();
  run_flow_graph_test();
  return 0;
}
