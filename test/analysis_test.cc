// Copyright 2018 Alex Horn. All rights reserved.
// Use of this source code is governed by a LICENSE.

#include "analysis_test.hh"
#include "ipv4_test_data.hh"

#include <analysis.hh>

using namespace nopticon;

static void check_duration(const slices_t &slices, duration_t d) {
  assert(slices.size() == 1);
  assert(slices.front().duration == d);
}

static void check_rank(const slices_t &slices, double rank) {
  constexpr double epsilon = 0.001;
  assert(slices.size() == 1);
  assert(slices.front().rank <= rank + epsilon);
  assert(slices.front().rank >= rank - epsilon);
}

static void test_network_summary() {
  spans_t spans;
  spans.push_back(10000);
  network_summary_t network_summary{spans, 8};
  auto &history_a = network_summary.history(1, 3, 5);
  assert(history_a.slices().size() == 1);
  history_a.start(1);
  history_a.stop(13);
  {
    const auto &ns = network_summary;
    check_duration(ns.slices(1, 3, 5), 12);
    check_duration(ns.slices(0, 3, 5), 0);
    check_duration(ns.slices(1, 2, 5), 0);
    check_duration(ns.slices(1, 3, 4), 0);
  }
  auto &history_b = network_summary.history(1, 4, 5);
  assert(history_b.slices().size() == 1);
  history_b.start(2);
  history_b.stop(17);
  {
    const auto &ns = network_summary;
    check_duration(ns.slices(1, 3, 5), 12);
    check_duration(ns.slices(1, 4, 5), 15);
    check_duration(ns.slices(0, 3, 5), 0);
    check_duration(ns.slices(1, 2, 5), 0);
    check_duration(ns.slices(1, 3, 4), 0);
  }
  auto &history_c = network_summary.history(1, 4, 7);
  assert(history_c.slices().size() == 1);
  history_c.start(5);
  history_c.stop(22);
  {
    const auto &ns = network_summary;
    check_duration(ns.slices(1, 3, 5), 12);
    check_duration(ns.slices(1, 4, 5), 15);
    check_duration(ns.slices(1, 4, 7), 17);
    check_duration(ns.slices(1, 2, 5), 0);
    check_duration(ns.slices(1, 3, 4), 0);
  }
}

static void test_history(history_t history) {
  // expect as input a history with a single span=20
  assert(history.slices().size() == 1);
  assert(history.slices().front().span() == 20);

  check_duration(history.slices(), 0);
  check_rank(history.slices(), 0.0);
  history.stop(9);
  check_duration(history.slices(), 0);
  check_rank(history.slices(), 0.0);
  history.start(3); // START: [3]
  check_duration(history.slices(), 0);
  check_rank(history.slices(), 0.0);
  history.start(2); // IGNORE
  check_duration(history.slices(), 0);
  check_rank(history.slices(), 0.0);
  history.start(4); // INGORE
  check_duration(history.slices(), 0);
  check_rank(history.slices(), 0.0);
  history.stop(7); // STOP: [3,7]
  check_duration(history.slices(), 4);
  check_rank(history.slices(), 1.0);
  history.stop(8); // IGNORE
  check_duration(history.slices(), 4);
  check_rank(history.slices(), 1.0);
  history.start(2); // IGNORE
  check_duration(history.slices(), 4);
  check_rank(history.slices(), 1.0);
  history.stop(8); // IGNORE
  check_duration(history.slices(), 4);
  check_rank(history.slices(), 1.0);
  history.start(12); // START [3,7,12]
  check_duration(history.slices(), 4);
  check_rank(history.slices(), 1.0);
  history.stop(9); // IGNORE
  check_duration(history.slices(), 4);
  check_rank(history.slices(), 1.0);
  history.stop(15); // STOP: [3,7,12,15]
  check_duration(history.slices(), 7);
  const double rank_0 = ((7 - 3) + (15 - 12)) / static_cast<float>(15 - 3);
  check_rank(history.slices(), rank_0);

  // extends array because span is still not filled yet
  history.start(18); // START: [3,7,12,15,18]
  check_duration(history.slices(), 7);
  check_rank(history.slices(), rank_0);
  history.stop(20); // STOP: [3,7,12,15,18,20]
  const double rank_1 =
      ((7 - 3) + (15 - 12) + (20 - 18)) / static_cast<float>(20 - 3);
  check_duration(history.slices(), 9);
  check_rank(history.slices(), rank_1);

  // exceeds span=20, so tail of slice is adjusted
  history.start(22); // START: [3,7,12,15,18,20,22]
  history.stop(25);  // STOP: [3,7,12,15,18,20,22,25]
  const double rank_2 =
      ((15 - 12) + (20 - 18) + (25 - 22)) / static_cast<float>(25 - 12);
  check_duration(history.slices(), 8);
  check_rank(history.slices(), rank_2);

  // span is filled, so no extension, instead wrap around in ring buffer
  history.start(28); // START: [28,7,12,15,18,20,22]
  history.stop(32);  // STOP: [28,32,12,15,18,20,22,25]
  const double rank_3 = ((32 - 28) + (15 - 12) + (20 - 18) + (25 - 22)) /
                        static_cast<float>(32 - 12);
  check_duration(history.slices(), 12);
  check_rank(history.slices(), rank_3);

  history.start(35); // START: [28,32,35,15,18,20,22]
  history.stop(37);  // STOP: [28,32,35,37,18,20,22,25]
  const double rank_4 = ((37 - 35) + (32 - 28) + (20 - 18) + (25 - 22)) /
                        static_cast<float>(37 - 18);
  check_duration(history.slices(), 11);
  check_rank(history.slices(), rank_4);
}

static void test_history() {
  spans_t spans;
  spans.push_back(20);
  test_history(history_t(spans, 3));
  test_history(history_t(spans, 2));
}

// a <- c
// .   ^:
// .  / :
// V /  V
// b <. d
static void test_loop_with_different_ip_prefixes() {
  const ip_addr_t a{0}, b{1}, c{2}, d{3};
  analysis_t analysis{4};

  analysis.insert_or_assign(ip_prefix_0_15, a, {b});
  assert(analysis.ok());

  analysis.insert_or_assign(ip_prefix_0_7, b, {c});
  assert(analysis.ok());

  analysis.insert_or_assign(ip_prefix_8_15, c, {d});
  assert(analysis.ok());

  analysis.insert_or_assign(ip_prefix_0_15, d, {b});
  assert(analysis.ok());

  analysis.insert_or_assign(ip_prefix_0_7, c, {a});
  assert(not analysis.ok());
  assert(analysis.loops_per_flow().size() == 1);
  auto &flow_tree = analysis.flow_graph().flow_tree();
  auto flow = flow_tree.find(ip_prefix_0_7);
  auto &loops = analysis.loops_per_flow().at(flow);
  assert(loops.size() == 1);
  auto &loop = loops.front();
  assert(loop == loop_t({a, b, c}));
}

// a <- c
// |   ^
// |  /
// V /
// b
static void test_loop() {
  const ip_addr_t a{0}, b{1}, c{2};
  analysis_t analysis{3};
  analysis.insert_or_assign(ip_prefix_0_15, a, {b});
  assert(analysis.ok());
  analysis.insert_or_assign(ip_prefix_0_15, b, {c});
  assert(analysis.ok());
  analysis.insert_or_assign(ip_prefix_0_15, c, {a});
  assert(not analysis.ok());
  assert(analysis.loops_per_flow().size() == 1);
  auto &flow_tree = analysis.flow_graph().flow_tree();
  auto flow = flow_tree.find(ip_prefix_0_15);
  auto &loops = analysis.loops_per_flow().at(flow);
  assert(loops.size() == 1);
  auto &loop = loops.front();
  assert(loop == loop_t({a, b, c}));
}

void run_analysis_test() {
  test_network_summary();
  test_history();
  test_loop();
  test_loop_with_different_ip_prefixes();
}
