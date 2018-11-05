// Copyright 2018 Alex Horn. All rights reserved.
// Use of this source code is governed by a LICENSE.

#include "ipv4_test_data.hh"

#include <algorithm>
#include <flow_graph.hh>
#include <iostream>
#include <sstream>
#include <tuple>

using namespace nopticon;

flows_t descendents_except(const_flow_t flow, const const_flows_t &flows) {
  assert(flow != nullptr);
  auto flow_iter = flow->iter();
  flows_t descendents;
  do {
    auto descendent = const_cast<flow_t>(flow_iter.ptr());
    if (flows.count(descendent) == 0) {
      descendents.insert(descendent);
    }
  } while (flow_iter.next());
  return descendents;
}

typedef std::tuple<bool, unsigned short> cmd_t;
typedef std::vector<cmd_t> cmd_vec_t;

static cmd_vec_t make_cmd_vec(const ip_prefix_vec_t &ip_prefix_vec,
                              unsigned short dup) {
  cmd_vec_t cmd_vec;
  assert(std::is_sorted(ip_prefix_vec.begin(), ip_prefix_vec.end(),
                        ip_prefix_order_t()));
  static_assert(false < true, "Expect 'false < true'");
  for (bool insert : {false, true}) {
    for (ip_prefix_vec_t::size_type i = 0; i < ip_prefix_vec.size(); ++i) {
      for (unsigned short j = 0; j < dup; ++j) {
        cmd_vec.emplace_back(insert, i);
      }
    }
  }
  assert(std::is_sorted(cmd_vec.begin(), cmd_vec.end()));
  return cmd_vec;
}

static void print(const ip_prefix_vec_t &ip_prefix_vec,
                  const cmd_vec_t &cmd_vec, int highlight_cmd_index = -1) {
  int cmd_index = 0;
  for (auto &cmd : cmd_vec) {
    const auto &ip_prefix = ip_prefix_vec.at(std::get<1>(cmd));
    std::cerr << ((cmd_index++ == highlight_cmd_index) ? "=> " : "   ");
    if (std::get<0>(cmd)) {
      std::cerr << "+(" << ip_prefix << ")" << std::endl;
    } else {
      std::cerr << "-(" << ip_prefix << ")" << std::endl;
    }
  }
}

static void run(flow_graph_t &flow_graph, const ip_prefix_vec_t &ip_prefix_vec,
                const cmd_vec_t &cmd_vec) {
  static const source_t source{ip_prefix_n_32.ip_addr};
  static const target_t target;

  bool ok;
  int cmd_index = 0;
  assert(cmd_vec.size() < std::numeric_limits<int>::max());
  affected_flows_t affected_flows;
  affected_flows.reserve(cmd_vec.size());
  for (auto &cmd : cmd_vec) {
    const auto &ip_prefix = ip_prefix_vec.at(std::get<1>(cmd));
    if (std::get<0>(cmd)) {
      ok = flow_graph.insert_or_assign(ip_prefix, source, target,
                                       affected_flows);
    } else {
      ok = flow_graph.erase(ip_prefix, source, affected_flows);
    }
    if (ok and affected_flows.empty()) {
      print(ip_prefix_vec, cmd_vec, cmd_index);
    }
    ++cmd_index;
    assert(not(ok and affected_flows.empty()));
    affected_flows.clear();
  }
}

static void test_print_ip_prefix() {
  {
    std::ostringstream sstream;
    sstream << ip_prefix_0_255;
    assert("0.0.0.0/24" == sstream.str());
  }
  {
    std::ostringstream sstream;
    sstream << ip_prefix_197_dot_157_dot_32_slash_19;
    assert("197.157.32.0/19" == sstream.str());
  }
  {
    std::ostringstream sstream;
    sstream << ip_prefix_0_0;
    assert("0.0.0.0/0" == sstream.str());
  }
  {
    std::ostringstream sstream;
    sstream << ip_prefix_n_32;
    assert("0.0.0.42/32" == sstream.str());
  }
}

// Assumes that flow_t::is_empty() is always false
static void test_flow_graph(const ip_prefix_vec_t &ip_prefix_vec,
                            unsigned dup) {
  cmd_vec_t cmd_vec = make_cmd_vec(ip_prefix_vec, dup);
  do {
    const_flows_t all_flows;
    flow_graph_t flow_graph;
    run(flow_graph, ip_prefix_vec, cmd_vec);
    for (auto rule_ref = flow_graph.rule_set().rbegin();
         rule_ref != flow_graph.rule_set().rend(); ++rule_ref) {
      auto flow = flow_graph.flow_tree().find(rule_ref->ip_prefix);
      assert(not flow->is_empty());
      auto flows = descendents_except(flow, all_flows);
      if (rule_ref->flows != flows) {
        print(ip_prefix_vec, cmd_vec);
      }
      assert(rule_ref->flows == flows);
      all_flows.insert(flows.begin(), flows.end());
    }
  } while (std::next_permutation(cmd_vec.begin(), cmd_vec.end()));
}

static void test_flow_info() {
  const ip_addr_t a{0}, b{1}, c{2}, d{3};

  const_flow_t flow;
  rule_ref_t rule_ref;
  flow_graph_t flow_graph;
  affected_flows_t affected_flows;
  rule_ref_per_source_t::const_iterator data_iter;

  auto &flow_tree = flow_graph.flow_tree();

  flow_graph.insert_or_assign(ip_prefix_0_15, a, {b}, affected_flows);
  assert(affected_flows.size() == 1);
  flow = flow_tree.find(ip_prefix_0_15);
  assert(affected_flows.back() == flow);
  rule_ref = flow_graph.find(ip_prefix_0_15, a);
  assert(rule_ref->flows.size() == 1);
  assert(*rule_ref->flows.begin() == flow);
  data_iter = flow->data.find(a);
  assert(data_iter != flow->data.end());
  assert(data_iter->second == rule_ref);

  affected_flows.clear();
  flow_graph.insert_or_assign(ip_prefix_0_7, b, {c}, affected_flows);
  assert(affected_flows.size() == 1);
  flow = flow_tree.find(ip_prefix_0_7);
  assert(affected_flows.back() == flow);
  rule_ref = flow_graph.find(ip_prefix_0_7, b);
  assert(rule_ref->flows.size() == 1);
  rule_ref = flow_graph.find(ip_prefix_0_15, a);
  assert(rule_ref->flows.size() == 2);
  flow = flow_tree.find(ip_prefix_0_15);
  data_iter = flow->data.find(a);
  assert(data_iter != flow->data.end());
  assert(data_iter->second == rule_ref);
  assert(flow->data.find(b) == flow->data.end());
  flow = flow_tree.find(ip_prefix_0_7);
  data_iter = flow->data.find(a);
  assert(data_iter != flow->data.end());
  assert(data_iter->second == rule_ref);
  rule_ref = flow_graph.find(ip_prefix_0_7, b);
  data_iter = flow->data.find(b);
  assert(data_iter != flow->data.end());
  assert(data_iter->second == rule_ref);

  affected_flows.clear();
  flow_graph.insert_or_assign(ip_prefix_8_15, c, {d}, affected_flows);
  assert(affected_flows.size() == 1);
  flow = flow_tree.find(ip_prefix_8_15);
  assert(affected_flows.back() == flow);
  rule_ref = flow_graph.find(ip_prefix_8_15, c);
  assert(rule_ref->flows.size() == 1);
  rule_ref = flow_graph.find(ip_prefix_0_15, a);
  assert(rule_ref->flows.size() == 3);
  flow = flow_tree.find(ip_prefix_0_15);
  data_iter = flow->data.find(a);
  assert(data_iter != flow->data.end());
  assert(data_iter->second == rule_ref);
  assert(flow->data.find(b) == flow->data.end());
  assert(flow->data.find(c) == flow->data.end());
  flow = flow_tree.find(ip_prefix_0_7);
  data_iter = flow->data.find(a);
  assert(data_iter != flow->data.end());
  assert(data_iter->second == rule_ref);
  flow = flow_tree.find(ip_prefix_8_15);
  data_iter = flow->data.find(a);
  assert(data_iter != flow->data.end());
  assert(data_iter->second == rule_ref);
  assert(flow->data.find(b) == flow->data.end());
  flow = flow_tree.find(ip_prefix_0_15);
  assert(flow->data.find(b) == flow->data.end());
  flow = flow_tree.find(ip_prefix_8_15);
  assert(flow->data.find(b) == flow->data.end());
  rule_ref = flow_graph.find(ip_prefix_8_15, c);
  data_iter = flow->data.find(c);
  assert(data_iter != flow->data.end());
  assert(data_iter->second == rule_ref);
  flow = flow_tree.find(ip_prefix_0_7);
  data_iter = flow->data.find(b);
  assert(data_iter != flow->data.end());
  rule_ref = flow_graph.find(ip_prefix_0_7, b);
  assert(data_iter->second == rule_ref);
}

void run_flow_graph_test() {
  test_print_ip_prefix();
  test_flow_info();
  test_flow_graph({ip_prefix_w, ip_prefix_x, ip_prefix_y, ip_prefix_z}, 1U);
  test_flow_graph({ip_prefix_u, ip_prefix_i, ip_prefix_j}, 2U);
}
