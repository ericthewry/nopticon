// Copyright 2018 Alex Horn. All rights reserved.
// Use of this source code is governed by a LICENSE.

#undef NDEBUG

#include "analysis_test.hh"
#include "flow_graph_test.hh"
#include "ipv4_test.hh"
#include <iostream>

int main() {
  std::cout << "Running IPv4 test" << std::endl;
  run_ipv4_test();
  std::cout << "..done" << std::endl
  << "Running Flow Graph Test" << std::endl;
  run_flow_graph_test();
  std::cout << "..done" << std::endl;
  std::cout << "Running Analysis Test" << std::endl;
  run_analysis_test();
  std::cout << "..done" << std::endl;
  std::cout << "ok" << std::endl;
  return 0;
}
