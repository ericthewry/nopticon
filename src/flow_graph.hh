// Copyright 2018 Alex Horn. All rights reserved.
// Use of this source code is governed by a LICENSE.

#pragma once

#include "ip_prefix_tree.hh"

#include <set>
#include <unordered_map>
#include <unordered_set>

namespace nopticon {

typedef uint32_t nid_t;
typedef nid_t source_t;
typedef std::vector<nid_t> target_t;

struct rule_t;
struct rule_order_t {
  ip_prefix_order_t ip_prefix_order;
  bool operator()(const rule_t &, const rule_t &) const noexcept;
};
typedef std::set<rule_t, rule_order_t> rule_set_t;
typedef rule_set_t::iterator rule_ref_t;
typedef std::unordered_map<source_t, rule_ref_t> rule_ref_per_source_t;
typedef ip_prefix_tree_t<rule_ref_per_source_t> flow_tree_t;
typedef typename flow_tree_t::ptr_t flow_t;
typedef typename flow_tree_t::const_ptr_t const_flow_t;
typedef std::unordered_set<flow_t> flows_t;
typedef std::unordered_set<const_flow_t> const_flows_t;

struct rule_t {
  ip_prefix_t ip_prefix;
  source_t source;

  mutable target_t target;
  mutable flows_t flows;

  rule_t(const ip_prefix_t &ip_prefix, source_t source)
      : ip_prefix{ip_prefix}, source{source}, target{}, flows{} {}
};

typedef std::vector<const_flow_t> affected_flows_t;

typedef flow_tree_t::id_t flow_id_t;

class flow_graph_t {
public:
  /// Returns true when a new rule has been created; false otherwise
  bool insert_or_assign(const ip_prefix_t &, source_t, const target_t &,
                        affected_flows_t &);

  /// Returns true if the rule existed; false otherwise
  bool erase(const ip_prefix_t &, source_t, affected_flows_t &);

  rule_ref_t find(const ip_prefix_t &, source_t) const;
  const rule_set_t &rule_set() const { return m_rule_set; }
  const flow_tree_t &flow_tree() const { return m_flow_tree; }

private:
  void insert_flow(rule_ref_t, flow_t);
  void reassign_flow(rule_ref_t, rule_ref_t, flow_t);

  rule_set_t m_rule_set;
  flow_tree_t m_flow_tree;
  flow_id_t m_next_flow_id = 1;
};

} // namespace nopticon
