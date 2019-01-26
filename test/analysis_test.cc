// Copyright 2018 Alex Horn. All rights reserved.
// Use of this source code is governed by a LICENSE.

#include "analysis_test.hh"
#include "ipv4_test_data.hh"

#include <analysis.hh>

#include <iostream>

#include <random>

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
  check_duration(history_3_5.slices(), 18);
  check_rank(reach_summary, history_3_5, 1.0);

  auto &history_4_5 = reach_summary.history(1, 4, 5);
  check_duration(history_4_5.slices(), 5);
  check_rank(reach_summary, history_4_5, 5 / static_cast<float>(19 - 1));

  auto &history_4_7 = reach_summary.history(1, 4, 7);
  check_duration(history_4_7.slices(), 0);
  check_rank(reach_summary, history_4_7,
             (19 - 7) / static_cast<float>(19 - 1));

  auto &history_2_3 = reach_summary.history(1, 2, 3);
  analysis.insert_or_assign(ip_prefix, 2, {3}, 13);
  analysis.erase(ip_prefix, 2, 81);
  check_duration(history_2_3.slices(), 68);
  check_rank(reach_summary, history_2_3, 1.0);

  analysis.insert_or_assign(ip_prefix, 2, {3}, 100);
  analysis.erase(ip_prefix, 2, 153);
  check_duration(history_2_3.slices(), 53);
  check_rank(reach_summary, history_2_3, 1.0);

  analysis.insert_or_assign(ip_prefix, 2, {3}, 170);
  analysis.erase(ip_prefix, 2, 184);
  check_duration(history_2_3.slices(), 14);
  check_rank(reach_summary, history_2_3, 14 / 18.0);
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

static timestamps_t simple_intersect(const timestamps_t &a, const timestamps_t &b) {
  if (a.empty() or b.empty()) {
    return {};
  }
  assert(std::is_sorted(a.begin(), a.end()));
  assert(std::is_sorted(b.begin(), b.end()));
  assert(!(a.size() & 1));
  assert(!(b.size() & 1));
  timestamps_t c;
  c.reserve(std::max(a.size(), b.size()));

  constexpr bool A = 0, B = 1;
  timestamps_t::size_type more_array[] = {a.size(), b.size()};
  timestamps_t::const_iterator iter_array[] = {a.cbegin(), b.cbegin()};
  timestamp_t low[] = {-1ULL, -1ULL}, high[] = {0ULL, 0ULL};

  auto advance = [&](bool index) {
    auto& _iter = iter_array[index];
    auto& _more = more_array[index];
    auto& _low = low[index];
    auto& _high = high[index];
    assert(2 <= _more);
    _more -= 2;
    _low = *(_iter++);
    _high = *(_iter++);
    assert(_low != 0);
    assert(_high != 0);
    assert(_low <= _high);
  };
  auto process_both_intervals = [&]() {
    if (low[A] <= high[B] and low[B] <= high[A]) {
      auto _low = std::max(low[A], low[B]);
      auto _high = std::min(high[A], high[B]);
      if (not c.empty() and c.back() == _low) {
        c.back() = _high;
      } else {
        c.push_back(_low);
        c.push_back(_high);
      }
    }
  };
  while (more_array[A] and more_array[B]) {
    if (high[A] < high[B]) {
      advance(A);
    } else {
      advance(B);
    }
    process_both_intervals();
  }
  const auto X = !more_array[A];
  while (more_array[X]) {
    advance(X);
    process_both_intervals();
  }
  return c;
}

static void test_intersection_of_timestamps() {
  const timestamps_t x{{3, 7}}, y{{5, 9}}, z{{4, 6}};
  assert(intersect(x, y) == timestamps_t({5, 7}));
  assert(intersect(y, z) == timestamps_t({5, 6}));
  const timestamps_t u{{1, 3, 5, 8, 9, 15}}, v{{1, 5}}, w{{5, 12}};
  assert(intersect(u, v) == timestamps_t({1, 3, 5, 5}));
  assert(intersect(u, w) == timestamps_t({5, 8, 9, 12}));
  const timestamps_t p{{1, 3, 5, 7, 8, 9}}, q{{2, 4, 6, 7}};
  assert(intersect(p, q) == timestamps_t({2, 3, 6, 7}));
  const timestamps_t i{{10, 17, 29, 35, 42, 53, 58, 70, 70, 81, 90, 99}};
  const timestamps_t j{{12, 44, 54, 70, 80, 99}};
  assert(intersect(i, j) == timestamps_t({12, 17, 29, 35, 42, 44, 58, 70, 80, 81, 90, 99}));
  assert(intersect(j, i) == intersect(i, j));
  for (auto a : {x, y, z, u, v, w, p, q}) {
    for (auto b : {x, y, z, u, v, w, p, q}) {
      assert(intersect(a, b) == intersect(b, a));
      for (auto c : {x, y, z, u, v, w, p, q}) {
        assert(intersect(c, intersect(a, b)) == intersect(intersect(c, a), b));
      }
    }
  }
  for (unsigned repeat = 0; repeat < 4096; ++repeat) {
    std::random_device rd;
    std::mt19937 gen{rd()};
    std::uniform_int_distribution<unsigned> dis{1, 65536};
    timestamps_t g, h;
    g.reserve(4096);
    h.reserve(4096);
    for (std::size_t k = 0; k < 4096; ++k) {
      g.push_back(dis(gen));
      h.push_back(dis(gen));
    }
    std::sort(g.begin(), g.end());
    std::sort(h.begin(), h.end());
    assert(intersect(g, h) == simple_intersect(g, h));
  }
}

static void test_slice_too_small() {
  spans_t spans;
  spans.push_back(20);
  history_t history{spans, 3};

  //[1,50]
  history.start(1);
  history.stop(50);
  check_duration(history.slices(), 49);

  history.reset();

  //[1,21]
  history.start(1);
  history.stop(21);
  check_duration(history.slices(),20);

  history.reset();

  // [1541089737329,1541089738324,1541089783864,1541089783886]
  history.start(1541089737329);
  history.stop(1541089738324);
  check_duration(history.slices(), 995);

  history.start(1541089783864);
  history.stop(1541089783886);
  check_duration(history.slices(), 22);

  history.reset();

  //[1,5,6,21]
  history.start(1);
  history.stop(5);
  history.start(6);
  history.stop(21);
  check_duration(history.slices(),19);

  history.reset();

  //[1,5,6,25,26,30]
  history.start(1);  // [1]
  history.stop(5);   // [1,5]
  history.start(6);  // [1,5,6]
  history.stop(25);  // [1,5,6,25]
  history.start(26); // [26,5,6,25]
  history.stop(30);  // [26,30,6,25]
  check_duration(history.slices(), 4);

  history.reset();

  // [1,5,6,15,20,45]
  history.start(1);
  history.stop(5);
  history.start(6);
  history.stop(15);
  history.start(20);
  history.stop(45);
  check_duration(history.slices(), 25);
}

static void test_path_preference_inference() {
  const ip_addr_t a{0}, b{1}, c{2}, d{3};
  spans_t spans;
  spans.push_back(20);

  analysis_t analysis{spans, 4};

  analysis.link_up(a, b, 1);
  analysis.link_down(a, b, 3);

  analysis.link_up(a, b, 6);
  analysis.link_down(a, b, 9);

  analysis.link_up(a, c, 1);
  analysis.link_down(a, c, 5);

  analysis.link_up(a, c, 7);
  analysis.link_down(a, c, 8);

  analysis.link_up(b, c, 1);
  analysis.link_down(b, c, 8);

  analysis.link_up(c, d, 2);
  analysis.link_down(c, d, 5);

  analysis.link_up(c, d, 6);
  analysis.link_down(c, d, 9);

  analysis.insert_or_assign(ip_prefix_64_127, a, {c}, 1);
  analysis.insert_or_assign(ip_prefix_64_127, c, {d}, 1);
  analysis.erase(ip_prefix_64_127, a, 9);

  analysis.insert_or_assign(ip_prefix_0_15, a, {b}, 2);
  analysis.insert_or_assign(ip_prefix_0_15, b, {c}, 2);
  analysis.insert_or_assign(ip_prefix_0_15, c, {d}, 2);
  analysis.insert_or_assign(ip_prefix_0_15, a, {c}, 3);
  analysis.insert_or_assign(ip_prefix_0_15, a, {b}, 6);
  analysis.erase(ip_prefix_0_15, a, 8);

  auto path_timestamps = analysis.path_preference_summary().get_path_timestamps();
  assert(path_timestamps[path_t({a, c, d})] == timestamps_t({2, 5, 7, 8}));
  assert(path_timestamps[path_t({a, b, c, d})] == timestamps_t({2, 3, 6, 8}));

  auto path_preferences = analysis.path_preferences();
  assert(path_preferences.size() == 3);
  {
    auto& record = path_preferences[0];
    assert(record.x_path == path_t({a, c, d}));
    assert(record.y_path == path_t({a, b, c, d}));
    assert(record.flow_id == 1);
    assert(record.rank >= 0.999);
    assert(record.rank <= 1.0);
  }
  {
    auto& record = path_preferences[1];
    assert(record.x_path == path_t({a, b, c, d}));
    assert(record.y_path == path_t({a, c, d}));
    assert(record.flow_id == 2);
    assert(record.rank >= 0.999);
    assert(record.rank <= 1.0);
  }
  {
    auto& record = path_preferences[2];
    assert(record.x_path == path_t({a, c, d}));
    assert(record.y_path == path_t({a, b, c, d}));
    assert(record.flow_id == 2);
    assert(record.rank >= 0);
    assert(record.rank <= 0.001);
  }
}

static void test_path_preference_with_link_events() {
  const std::size_t number_of_nodes = 3;
  const ip_addr_t a{0}, b{1}, c{2};
  const ip_prefix_t ip_prefix = ip_prefix_64_127;

  spans_t spans;
  spans.push_back(900000);
  analysis_t analysis{spans, number_of_nodes};
  analysis.link_up(a, b, 383548);
  analysis.insert_or_assign(ip_prefix, a, {b}, 383550);
  analysis.insert_or_assign(ip_prefix, c, {a}, 417835);
  analysis.link_up(c, a, 739192);
  auto path_preferences = analysis.path_preferences();
  assert(path_preferences.empty());
}

void run_analysis_test() {
  test_slice_too_small();
  test_reach_summary();
  test_history();
  test_loop();
  test_loop_with_different_ip_prefixes();
  test_analysis();
  test_refresh();
  test_intersection_of_timestamps();
  test_path_preference_inference();
  test_path_preference_with_link_events();
}
