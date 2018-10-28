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

static void check_rank(const reach_summary_t &reach_summary,
                       const history_t &history, double rank) {
  constexpr double epsilon = 0.001;
  auto &slices = history.slices();
  assert(slices.size() == 1);
  auto ranks = reach_summary.ranks(history);
  assert(ranks.size() == 1);
  auto slice_rank = ranks.front();
  assert(slice_rank <= 1.0);
  assert(slice_rank <= rank + epsilon);
  assert(slice_rank >= rank - epsilon);
}

static void test_reach_summary() {
  spans_t spans;
  spans.push_back(10000);
  reach_summary_t reach_summary{spans, 8};
  auto &history_a = reach_summary.history(1, 3, 5);
  assert(history_a.slices().size() == 1);
  history_a.start(1);
  history_a.stop(13);
  {
    const auto &rs = reach_summary;
    check_duration(rs.slices(1, 3, 5), 12);
    check_duration(rs.slices(0, 3, 5), 0);
    check_duration(rs.slices(1, 2, 5), 0);
    check_duration(rs.slices(1, 3, 4), 0);
  }
  auto &history_b = reach_summary.history(1, 4, 5);
  assert(history_b.slices().size() == 1);
  history_b.start(2);
  history_b.stop(17);
  {
    const auto &rs = reach_summary;
    check_duration(rs.slices(1, 3, 5), 12);
    check_duration(rs.slices(1, 4, 5), 15);
    check_duration(rs.slices(0, 3, 5), 0);
    check_duration(rs.slices(1, 2, 5), 0);
    check_duration(rs.slices(1, 3, 4), 0);
  }
  auto &history_c = reach_summary.history(1, 4, 7);
  assert(history_c.slices().size() == 1);
  history_c.start(5);
  history_c.stop(22);
  {
    const auto &rs = reach_summary;
    check_duration(rs.slices(1, 3, 5), 12);
    check_duration(rs.slices(1, 4, 5), 15);
    check_duration(rs.slices(1, 4, 7), 17);
    check_duration(rs.slices(1, 2, 5), 0);
    check_duration(rs.slices(1, 3, 4), 0);
  }
}

static void test_history(history_t history) {
  // expect as input a history with a single span=20
  assert(history.slices().size() == 1);
  assert(history.slices().front().span() == 20);

  check_duration(history.slices(), 0);
  history.stop(9);
  check_duration(history.slices(), 0);
  history.start(3); // START: [3]
  check_duration(history.slices(), 0);
  history.start(2); // IGNORE
  check_duration(history.slices(), 0);
  history.start(4); // INGORE
  check_duration(history.slices(), 0);
  history.stop(7); // STOP: [3,7]
  check_duration(history.slices(), 4);
  history.stop(8); // IGNORE
  check_duration(history.slices(), 4);
  history.start(2); // IGNORE
  check_duration(history.slices(), 4);
  history.stop(8); // IGNORE
  check_duration(history.slices(), 4);
  history.start(12); // START [3,7,12]
  check_duration(history.slices(), 4);
  history.stop(9); // IGNORE
  check_duration(history.slices(), 4);
  history.stop(15); // STOP: [3,7,12,15]
  check_duration(history.slices(), 7);

  // extends array because span is still not filled yet
  history.start(18); // START: [3,7,12,15,18]
  check_duration(history.slices(), 7);
  history.stop(20); // STOP: [3,7,12,15,18,20]
  check_duration(history.slices(), 9);

  // exceeds span=20, so tail of slice is adjusted
  history.start(22); // START: [3,7,12,15,18,20,22]
  history.stop(25);  // STOP: [3,7,12,15,18,20,22,25]
  check_duration(history.slices(), 8);

  // span is filled, so no extersion, irstead wrap around in ring buffer
  history.start(28); // START: [28,7,12,15,18,20,22]
  history.stop(32);  // STOP: [28,32,12,15,18,20,22,25]
  check_duration(history.slices(), 12);

  history.start(35); // START: [28,32,35,15,18,20,22]
  history.stop(37);  // STOP: [28,32,35,37,18,20,22,25]
  check_duration(history.slices(), 11);
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

static void test_analysis() {
  const std::size_t number_of_nodes = 8;
  const ip_prefix_t ip_prefix = ip_prefix_64_127;

  spans_t spans{18};
  analysis_t analysis{spans, number_of_nodes};
  analysis.insert_or_assign(ip_prefix, 3, {5}, 1);
  analysis.insert_or_assign(ip_prefix, 4, {5}, 2);
  // idempotent
  analysis.insert_or_assign(ip_prefix, 4, {5}, 2);
  analysis.insert_or_assign(ip_prefix, 4, {7}, 7);
  analysis.erase(ip_prefix, 3, 19);
  auto &reach_summary = analysis.reach_summary();
  auto &history_3_5 = reach_summary.history(1, 3, 5);
  assert(history_3_5.slices().size() == 1);
  assert(history_3_5.slices().front().duration == 18);
  check_rank(reach_summary, history_3_5, 1.0);

  auto &history_4_5 = reach_summary.history(1, 4, 5);
  assert(history_4_5.slices().size() == 1);
  assert(history_4_5.slices().front().duration == 5);
  check_rank(reach_summary, history_4_5, 5 / static_cast<float>(19 - 1));

  auto &history_4_7 = reach_summary.history(1, 4, 7);
  assert(history_4_7.slices().size() == 1);
  assert(history_4_7.slices().front().duration == 0);
  check_rank(reach_summary, history_4_7,
             (19 - 7) / static_cast<float>(19 - 1));
}

static void test_refresh() {
  const std::size_t number_of_nodes = 5;
  const ip_prefix_t ip_prefix = ip_prefix_64_127;

  spans_t spans{5};
  analysis_t analysis{spans, number_of_nodes};

  analysis.insert_or_assign(ip_prefix, 0, {1}, 1);
  analysis.insert_or_assign(ip_prefix, 1, {2}, 2);
  analysis.insert_or_assign(ip_prefix, 2, {3}, 3);
  {
    auto &rs = analysis.reach_summary();
    assert(rs.history(1, 0, 1).timestamps(4) == timestamps_t({1, 4}));
    assert(rs.history(1, 1, 2).timestamps(4) == timestamps_t({2, 4}));
    assert(rs.history(1, 2, 3).timestamps(4) == timestamps_t({3, 4}));
    assert(rs.history(1, 0, 3).timestamps(4) == timestamps_t({3, 4}));
  }
  analysis.refresh_reach_summary(5);
  analysis.insert_or_assign(ip_prefix, 0, {3}, 6);
  {
    auto &rs = analysis.reach_summary();
    assert(rs.history(1, 0, 1).timestamps(7) == timestamps_t({5, 6}));
    assert(rs.history(1, 1, 2).timestamps(7) == timestamps_t({5, 7}));
    assert(rs.history(1, 2, 3).timestamps(7) == timestamps_t({5, 7}));
    assert(rs.history(1, 0, 3).timestamps(7) == timestamps_t({5, 7}));
  }
  analysis.insert_or_assign(ip_prefix, 0, {1}, 7);
  {
    auto &rs = analysis.reach_summary();
    assert(rs.history(1, 0, 1).timestamps(8) == timestamps_t({5, 6, 7, 8}));
    assert(rs.history(1, 1, 2).timestamps(8) == timestamps_t({5, 8}));
    assert(rs.history(1, 2, 3).timestamps(8) == timestamps_t({5, 8}));
    assert(rs.history(1, 0, 3).timestamps(8) == timestamps_t({5, 8}));
  }
  analysis.insert_or_assign(ip_prefix, 0, {3}, 9);
  {
    auto &rs = analysis.reach_summary();
    assert(rs.history(1, 0, 1).timestamps(10) == timestamps_t({5, 6, 7, 9}));
    assert(rs.history(1, 1, 2).timestamps(10) == timestamps_t({5, 10}));
    assert(rs.history(1, 2, 3).timestamps(10) == timestamps_t({5, 10}));
    assert(rs.history(1, 0, 3).timestamps(10) == timestamps_t({5, 10}));
  }
  analysis.refresh_reach_summary(11);
  {
    auto &rs = analysis.reach_summary();
    assert(rs.history(1, 0, 1).timestamps(12).empty());
    assert(rs.history(1, 1, 2).timestamps(12) == timestamps_t({11, 12}));
    assert(rs.history(1, 2, 3).timestamps(12) == timestamps_t({11, 12}));
    assert(rs.history(1, 0, 3).timestamps(12) == timestamps_t({11, 12}));
  }
  analysis.insert_or_assign(ip_prefix, 0, {1}, 15);
  analysis.erase(ip_prefix, 1, 15);
  {
    auto &rs = analysis.reach_summary();
    assert(rs.history(1, 0, 1).timestamps(17) == timestamps_t({15, 17}));
    assert(rs.history(1, 1, 2).timestamps(17) == timestamps_t({11, 15}));
    assert(rs.history(1, 2, 3).timestamps(17) == timestamps_t({11, 17}));
    assert(rs.history(1, 0, 3).timestamps(17) == timestamps_t({11, 15}));
  }
}

void run_analysis_test() {
  test_reach_summary();
  test_history();
  test_loop();
  test_loop_with_different_ip_prefixes();
  test_analysis();
  test_refresh();
}
