// Copyright 2018 Alex Horn. All rights reserved.
// Use of this source code is governed by a LICENSE.

#pragma once

#include "flow_graph.hh"

namespace nopticon {

typedef std::vector<nid_t> loop_t;
typedef std::vector<loop_t> loops_t;
typedef std::unordered_map<const_flow_t, loops_t> loops_per_flow_t;

void find_loops(source_t, const affected_flows_t &, loops_per_flow_t &);

bool check_loop(const_flow_t, const loop_t &);

/// Microseconds
typedef uint64_t duration_t;
typedef uint64_t timestamp_t;

class slice_t {
public:
  slice_t(duration_t span) : m_span{span} {}

  /// total slice-time in which a property held
  duration_t duration = 0;

  /// normalized duration in which a property held
  float rank = 0;

  /// total permittable time duration of the slice
  duration_t span() const noexcept { return m_span; }

private:
  // non-const only to allow for history vector resizing
  duration_t m_span;

  friend class history_t;

  // end of slice, always an even number
  std::size_t tail = 0;
};

typedef std::vector<slice_t> slices_t;
typedef std::vector<duration_t> spans_t;
typedef std::vector<timestamp_t> timestamps_t;

/// A sliced, sliding time window
class history_t {
public:
  /// Spans must be sorted in increasing order
  history_t(const spans_t &spans, uint8_t exponent = 7)
      : m_time_window(1 << exponent), m_head{/* stop */ m_time_window.size() -
                                             1} {
    assert(1 < exponent and exponent <= 12);
    assert(std::is_sorted(spans.begin(), spans.end()));
    for (auto span : spans) {
      m_slices.emplace_back(span);
    }
  }

  /// Mark the time at which a property starts to hold
  void start(timestamp_t);

  /// Mark the time at which a property stops to hold
  void stop(timestamp_t);

  /// Reset each slice
  void reset() noexcept;

  /// Ordered according to their span, from shortest to longest
  const slices_t &slices() const noexcept { return m_slices; }

  /// Size is always be a power of two
  const timestamps_t &time_window() const noexcept { return m_time_window; }

private:
  std::size_t index(std::size_t i) const {
    return i & (m_time_window.size() - 1);
  }

  void update_duration(bool, timestamp_t);

  timestamps_t m_time_window;
  std::size_t m_head;
  slices_t m_slices;
};

typedef std::vector<history_t> history_vec_t;

class network_summary_t {
public:
  const spans_t spans;
  const std::size_t number_of_nodes;
  network_summary_t(std::size_t);
  network_summary_t(const spans_t &, std::size_t);

  void reset() noexcept;

  /// Ordered according to their span, from longest to shortest
  const slices_t &slices(flow_id_t, nid_t, nid_t) const;

  history_vec_t &flow(flow_id_t);
  history_t &history(flow_id_t, nid_t, nid_t);

private:
  typedef std::vector<history_vec_t> tensor_t;
  tensor_t m_tensor;

  inline std::size_t make_index(nid_t s, nid_t t) const {
    return number_of_nodes * s + t;
  }
};

class analysis_t {
public:
  constexpr static std::size_t MAX_NUMBER_OF_NODES = 4096;

  analysis_t(std::size_t number_of_nodes)
      : m_network_summary{spans_t{}, number_of_nodes} {}

  analysis_t(const spans_t &spans, std::size_t number_of_nodes)
      : m_network_summary{spans, number_of_nodes} {}

  /// Returns true when a new rule has been created; false otherwise
  bool insert_or_assign(const ip_prefix_t &, source_t, const target_t &,
                        timestamp_t current = 0);

  /// Returns true if the rule existed; false otherwise
  bool erase(const ip_prefix_t &, source_t, timestamp_t current = 0);

  bool ok() const noexcept { return m_loops_per_flow.empty(); }

  const flow_graph_t &flow_graph() const noexcept { return m_flow_graph; }

  void reset_network_summary() noexcept { m_network_summary.reset(); }

  const network_summary_t &network_summary() const noexcept {
    return m_network_summary;
  }

  const loops_per_flow_t &loops_per_flow() const noexcept {
    return m_loops_per_flow;
  }

  const affected_flows_t &affected_flows() const noexcept {
    return m_affected_flows;
  }

private:
  void clean_up();
  void update_network_summary(timestamp_t);

  flow_graph_t m_flow_graph;
  affected_flows_t m_affected_flows;
  loops_per_flow_t m_loops_per_flow;
  network_summary_t m_network_summary;
};

} // namespace nopticon
