// Copyright 2018 Alex Horn. All rights reserved.
// Use of this source code is governed by a LICENSE.

#include "analysis.hh"
#include <bitset>
#include <cstdlib>
#include <iostream>

namespace nopticon {

void history_t::refresh(timestamp_t timestamp) noexcept {
  // If we're in 'stop', then set tail to new start;
  // otherwise, cause tail to catch up with head.
  bool is_stop = m_head & 1;
  auto h = is_stop ? index(m_head + 1) : m_head;
  for (auto& slice : m_slices) {
    slice.duration = 0;
    slice.tail = h;
    assert(!(h & 1));
  }
  if (not is_stop) {
    m_time_window[h] = timestamp;
  }
}

rank_t history_t::rank(const slice_t &slice, timestamp_t global_start,
                       timestamp_t global_stop) const noexcept {
  constexpr float zero_div_guard = 0.00001;
  auto duration = slice.duration;
  assert(global_start <= global_start);
  // Timestamps of non-empty histories are non-decreasing.
  assert(duration == 0 or oldest_start_time(slice) <= newest_time());
  if (!(m_head & 1) and newest_time() <= global_stop) {
    // We're in 'start' and need to add a missing 'stop'.
    duration += global_stop - newest_time();
  }
  auto span = std::min(slice.span(), global_stop - global_start);
  return duration / (span + zero_div_guard);
}

void history_t::update_duration(bool is_stop, timestamp_t current) {
  assert(current != 0);
  // Start: 0, 2, 4, ...
  // Stop: 1, 3, 5, ...
  auto newest = m_time_window.at(m_head);
  if ((m_head & 1) == is_stop) {
    // - We're currently in 'start' or 'stop' and got
    //   another start or stop request, respectively;
    // - Stop requests for which there is no start.
    return;
  }
  // currently in 'start' but need to be in 'stop', or vice versa
  if (newest >= current) {
    // ignore simultaneous and out-of-order arrivals
    return;
  }
  m_time_window.at(m_head = index(m_head + 1)) = current;
  if (is_stop) {
    assert(m_head & 1); // current head is a start idx, expecting a stop
    auto next_head = index(m_head + 1);
    for (auto &slice : m_slices) {
      auto &d = slice.duration;
      auto &tail = slice.tail;
      assert(!(tail & 1));
      d += current - newest;
      auto actual_span = slice.span();
      for (;;) {
        assert(!(tail & 1)); // tail is a stop idx (even)
        auto oldest_start = oldest_start_time(slice);
        assert(oldest_start != 0);
        assert(oldest_start <= newest);
        actual_span = current - oldest_start;
        if (tail == next_head) {
          next_head = m_time_window.size();
          assert(m_head + 1 == next_head);
          m_time_window.resize(m_time_window.size() << 1);

          // unprovable, but something we'd like
          assert(m_time_window.size() <= (1 << 10));
        }

        if (actual_span <= slice.span()) {
          break;
        }
        auto oldest_stop = m_time_window.at(index(tail + 1));
        assert(oldest_start <= oldest_stop);
        d -= oldest_stop - oldest_start;
        if (tail + 1 == m_head) {
          std::cerr << "Error: slice is too small\n";
          std::exit(ERROR_SLICE_TOO_SMALL);
        }
        tail = index(tail + 2);
      }
      assert(actual_span <= slice.span());
    }
  }
}

void history_t::start(timestamp_t current) { update_duration(false, current); }
void history_t::stop(timestamp_t current) { update_duration(true, current); }

timestamps_t history_t::timestamps(timestamp_t global_end) const noexcept {
  duration_t duration = 0;
  auto tail = m_time_window.size();
  for (auto& slice : m_slices) {
    if (slice.duration >= duration) {
      tail = slice.tail;
    }
  }
  if (tail >= m_time_window.size() or ((m_head & 1) and index(m_head + 1) == tail)) {
    return {};
  }
  auto i = tail;
  timestamps_t vec;
  vec.reserve(m_time_window.size());
  for (;;) {
    vec.push_back(m_time_window[i]);
    i = index(i + 1);
    if (vec.back() > m_time_window[i] or i == tail) {
      break;
    }
  }
  if (vec.size() & 1) {
    vec.push_back(global_end);
  }
  return vec;
}

void history_t::reset() noexcept {
  m_head = m_time_window.size() - 1;
  for (auto &slice : m_slices) {
    slice.duration = 0;
    slice.tail = 0;
  }
}

reach_summary_t::reach_summary_t(std::size_t number_of_nodes)
    : spans{}, number_of_nodes{number_of_nodes} {
  assert(number_of_nodes <= analysis_t::MAX_NUMBER_OF_NODES);
}

reach_summary_t::reach_summary_t(const spans_t &spans,
                                     std::size_t number_of_nodes)
    : spans{spans}, number_of_nodes{number_of_nodes} {
  assert(number_of_nodes <= analysis_t::MAX_NUMBER_OF_NODES);
}

void reach_summary_t::reset() noexcept {
  for (auto &history_vec : m_tensor) {
    for (auto &history : history_vec) {
      history.reset();
    }
  }
}

void reach_summary_t::refresh(timestamp_t timestamp) noexcept {
  if (global_stop < timestamp) {
    global_stop = timestamp;
  }
  for (auto &history_vec : m_tensor) {
    for (auto &history : history_vec) {
      history.refresh(timestamp);
    }
  }
}

ranks_t reach_summary_t::ranks(const history_t &history) const {
  ranks_t ranks;
  ranks.reserve(history.slices().size());
  for (auto &slice : history.slices()) {
    ranks.push_back(history.rank(slice, global_start, global_stop));
  }
  return ranks;
}

const slices_t &reach_summary_t::slices(flow_id_t flow_id, nid_t s,
                                          nid_t t) const {
  static slices_t s_empty_slices;
  if (flow_id >= m_tensor.size()) {
    return s_empty_slices;
  }
  auto &history_vec = m_tensor[flow_id];
  auto index = make_index(s, t);
  if (index >= history_vec.size()) {
    return s_empty_slices;
  }
  return history_vec[index].slices();
}

history_vec_t &reach_summary_t::history_vec(flow_id_t flow_id) {
  if (flow_id >= m_tensor.size()) {
    auto old_size = m_tensor.size();
    m_tensor.resize((flow_id + 1) << 1);
    for (std::size_t i = old_size; i < m_tensor.size(); ++i) {
      assert(m_tensor[i].empty());
      history_t history{spans};
      m_tensor[i].resize(number_of_nodes * number_of_nodes, history);
    }
  }
  assert(flow_id < m_tensor.size());
  return m_tensor[flow_id];
}

const history_t &reach_summary_t::history(flow_id_t flow_id, nid_t s,
                                            nid_t t) const {
  static history_t s_empty_history{spans_t{}};
  if (flow_id >= m_tensor.size()) {
    return s_empty_history;
  }
  auto &history_vec = m_tensor[flow_id];
  auto index = make_index(s, t);
  if (index >= history_vec.size()) {
    return s_empty_history;
  }
  return history_vec[index];
}

history_t &reach_summary_t::history(flow_id_t flow_id, nid_t s, nid_t t) {
  auto &hv = history_vec(flow_id);
  auto index = make_index(s, t);
  assert(index < hv.size());
  return hv[index];
}

void find_loops(source_t start, const affected_flows_t &affected_flows,
                loops_per_flow_t &loops_per_flow) {
  ip_addr_vec_t stack, path;
  std::unordered_set<source_t> seen;
  for (auto flow : affected_flows) {
    assert(stack.empty());
    assert(seen.empty());
    assert(path.empty());

    auto &rule_ref_per_source = flow->data;
    stack.reserve(rule_ref_per_source.size() + 1);
    path.reserve(rule_ref_per_source.size() + 1);
    stack.push_back(start);
    while (not stack.empty()) {
      auto n = stack.back();
      stack.pop_back();
      auto rule_ref_per_source_iter = rule_ref_per_source.find(n);
      if (rule_ref_per_source_iter == rule_ref_per_source.end()) {
        if (path.empty()) {
          assert(stack.empty());
          break;
        }
        path.pop_back();
        continue;
      }
      auto rule_ref = rule_ref_per_source_iter->second;
      auto insert_result = seen.insert(n);
      if (not nopticon::ok(insert_result)) {
        assert(not path.empty());
        auto min_iter = std::min_element(path.begin(), path.end());
        std::rotate(path.begin(), min_iter, path.end());
        loops_per_flow[flow].push_back(std::move(path));
        path = ip_addr_vec_t();
        stack.clear();
        break;
      }
      path.push_back(n);
      stack.insert(stack.end(), rule_ref->target.begin(),
                   rule_ref->target.end());
    }
    seen.clear();
    path.clear();
  }
}

static bool is_connected(const rule_ref_per_source_t &rule_ref_per_source,
                         nid_t source, nid_t target) {
  auto rule_ref_per_source_iter = rule_ref_per_source.find(source);
  if (rule_ref_per_source_iter == rule_ref_per_source.end()) {
    return false;
  }
  auto rule_ref = rule_ref_per_source_iter->second;
  auto target_iter =
      std::find(rule_ref->target.begin(), rule_ref->target.end(), target);
  return target_iter != rule_ref->target.end();
}

bool check_loop(const_flow_t flow, const loop_t &loop) {
  auto &rule_ref_per_source = flow->data;
  auto loop_iter = loop.begin();
  assert(loop_iter != loop.end());
  for (; std::next(loop_iter) != loop.end(); ++loop_iter) {
    if (not is_connected(rule_ref_per_source, *loop_iter,
                         *std::next(loop_iter))) {
      return false;
    }
  }
  return is_connected(rule_ref_per_source, loop.back(), loop.front());
}

void analysis_t::clean_up() {
  for (auto flow : m_affected_flows) {
    auto loops_per_flow_iter = m_loops_per_flow.find(flow);
    if (loops_per_flow_iter == m_loops_per_flow.end()) {
      continue;
    }
    loops_t loops;
    for (auto &loop : loops_per_flow_iter->second) {
      if (check_loop(flow, loop)) {
        loops.push_back(std::move(loop));
      }
    }
    if (loops.empty()) {
      m_loops_per_flow.erase(flow);
    } else {
      m_loops_per_flow[flow] = std::move(loops);
    }
  }
}

void analysis_t::update_reach_summary(timestamp_t timestamp) {
  ip_addr_vec_t stack;
  std::bitset<MAX_NUMBER_OF_NODES> bitset;
  stack.reserve(m_reach_summary.number_of_nodes);

  if (timestamp < m_reach_summary.global_start) {
    m_reach_summary.global_start = timestamp;
  }
  if (m_reach_summary.global_stop < timestamp) {
    m_reach_summary.global_stop = timestamp;
  }

  for (auto flow : m_affected_flows) {
    assert(stack.empty());
    auto &history_vec = m_reach_summary.history_vec(flow->id);
    auto &rule_ref_per_source = flow->data;
    for (auto &kv : rule_ref_per_source) {
      assert(stack.empty());
      auto start = kv.first;
      auto base_index = m_reach_summary.number_of_nodes * start;
      stack.push_back(start);
      while (not stack.empty()) {
        auto n = stack.back();
        stack.pop_back();
        auto rule_ref_per_source_iter = rule_ref_per_source.find(n);
        if (rule_ref_per_source_iter == rule_ref_per_source.end()) {
          continue;
        }
        auto rule_ref = rule_ref_per_source_iter->second;
        for (auto t : rule_ref->target) {
          auto index = base_index + t;
          assert(index < history_vec.size());
          if (bitset.test(t)) {
            continue;
          }
          auto &history = history_vec[index];
          history.start(timestamp);
          history.request_stop = false;
          bitset.set(t);
          stack.push_back(t);
        }
      }
      bitset.reset();
    }
    for (auto &history : history_vec) {
      // stop requests for histories that just got started are no-ops
      if (history.request_stop) {
        history.stop(timestamp);
      }
      history.request_stop = true;
    }
  }
}

bool analysis_t::insert_or_assign(const ip_prefix_t &ip_prefix, source_t source,
                                  const target_t &new_target,
                                  timestamp_t timestamp) {
  m_affected_flows.clear();
  bool status = m_flow_graph.insert_or_assign(ip_prefix, source, new_target,
                                              m_affected_flows);
  clean_up();
  find_loops(source, m_affected_flows, m_loops_per_flow);
  if (timestamp != 0) {
    update_reach_summary(timestamp);
  }
  return status;
}

bool analysis_t::erase(const ip_prefix_t &ip_prefix, source_t source,
                       timestamp_t timestamp) {
  m_affected_flows.clear();
  bool status = m_flow_graph.erase(ip_prefix, source, m_affected_flows);
  clean_up();
  find_loops(source, m_affected_flows, m_loops_per_flow);
  if (timestamp != 0) {
    update_reach_summary(timestamp);
  }
  return status;
}

} // namespace nopticon
