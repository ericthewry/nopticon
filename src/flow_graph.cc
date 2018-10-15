// Copyright 2018 Alex Horn. All rights reserved.
// Use of this source code is governed by a LICENSE.

#include "flow_graph.hh"

namespace nopticon {

inline bool rule_order_t::operator()(const rule_t &x, const rule_t &y) const
    noexcept {
  if (x.source == y.source) {
    return ip_prefix_order(x.ip_prefix, y.ip_prefix);
  }
  return x.source < y.source;
}

rule_ref_t flow_graph_t::find(const ip_prefix_t &ip_prefix,
                              source_t source) const {
  return m_rule_set.find(rule_t(ip_prefix, source));
}

void flow_graph_t::insert_flow(rule_ref_t rule_ref, flow_t flow) {
  auto emplace_result = flow->data.emplace(rule_ref->source, rule_ref);
  assert(ok(emplace_result));
  auto insert_result = rule_ref->flows.insert(flow);
  assert(ok(insert_result));
}

void flow_graph_t::reassign_flow(rule_ref_t current_owner, rule_ref_t rule_ref,
                                 flow_t flow) {
  assert(current_owner->ip_prefix != flow->ip_prefix);
  assert(subset(rule_ref->ip_prefix, current_owner->ip_prefix));
  auto erase_result = current_owner->flows.erase(flow);
  assert(erase_result != 0);
  flow->data.at(rule_ref->source) = rule_ref;
  auto insert_result = rule_ref->flows.insert(flow);
  assert(ok(insert_result));
}

static void insert_flows(affected_flows_t &affected_flows,
                         const flows_t &flows) {
  for (auto flow : flows) {
    affected_flows.push_back(flow);
  }
}

bool flow_graph_t::insert_or_assign(const ip_prefix_t &ip_prefix,
                                    source_t source, const target_t &new_target,
                                    affected_flows_t &affected_flows) {
  rule_ref_t rule_ref;
  {
    rule_t new_rule{ip_prefix, source};
    rule_ref = m_rule_set.lower_bound(new_rule);
    if (rule_ref != m_rule_set.end() and
        not m_rule_set.key_comp()(new_rule, *rule_ref)) {
      assert(ip_prefix == rule_ref->ip_prefix);
      assert(source == rule_ref->source);
      if (new_target != rule_ref->target) {
        rule_ref->target = new_target;
        insert_flows(affected_flows, rule_ref->flows);
      }
      return false;
    }

    rule_ref = m_rule_set.insert(rule_ref, std::move(new_rule));
    rule_ref->target = new_target;
    assert(ip_prefix == rule_ref->ip_prefix);
    assert(source == rule_ref->source);
  }
  flow_t parent;
  auto &flow_tree = m_flow_tree.insert(ip_prefix, m_next_flow_id, parent);
  if (flow_tree.id == m_next_flow_id) {
    m_next_flow_id++;
    flow_tree.data = parent->data;
    for (auto &pair : flow_tree.data) {
      pair.second->flows.insert(&flow_tree);
    }
  }
  auto flow_tree_iter = flow_tree.iter();
  do {
    auto flow = flow_tree_iter.ptr();
    assert(flow != nullptr);
    auto data_iter = flow->data.find(source);
    if (data_iter == flow->data.end()) {
      insert_flow(rule_ref, flow);
      affected_flows.push_back(flow);
    } else {
      auto current_owner = data_iter->second;
      if (subset(ip_prefix, current_owner->ip_prefix)) {
        reassign_flow(current_owner, rule_ref, flow);
        affected_flows.push_back(flow);
      }
    }
  } while (flow_tree_iter.next());
  assert(not affected_flows.empty());
  return true;
}

bool flow_graph_t::erase(const ip_prefix_t &ip_prefix, source_t source,
                         affected_flows_t &affected_flows) {
  flow_t parent_flow = nullptr;
  rule_ref_t rule_ref, parent_rule_ref;
  {
    rule_t erase_rule{ip_prefix, source};
    rule_ref = m_rule_set.find(erase_rule);
    if (rule_ref == m_rule_set.end()) {
      return false;
    }
  }
  assert(ip_prefix == rule_ref->ip_prefix);
  assert(source == rule_ref->source);
  {
    std::vector<flow_t> p_flows;
    p_flows.reserve(ip_prefix_t::MAX_LEN >> 1);
    ip_prefix_t p_ip_prefix{ip_prefix};
    auto flow = m_flow_tree.find(ip_prefix, p_flows);
    assert(flow != nullptr);
    assert(flow->ip_prefix == ip_prefix);
    assert(flow->data.at(source) == rule_ref);
    for (auto iter = p_flows.rbegin(); iter != p_flows.rend(); ++iter) {
      auto p_flow = *iter;
      assert(p_flow != nullptr);
      assert(subset(p_ip_prefix, p_flow->ip_prefix));
      p_ip_prefix = p_flow->ip_prefix;
      auto data_iter = p_flow->data.find(source);
      if (data_iter != p_flow->data.end()) {
        parent_flow = p_flow;
        parent_rule_ref = data_iter->second;
        break;
      }
    }
  }
  if (parent_flow == nullptr) {
    for (auto flow : rule_ref->flows) {
      flow->data.erase(source);
    }
  } else {
    assert(subset(parent_flow->ip_prefix, parent_rule_ref->ip_prefix));
    assert(subset(rule_ref->ip_prefix, parent_flow->ip_prefix));
    assert(parent_rule_ref->source == source);
    assert(parent_rule_ref->flows.count(parent_flow) != 0);
    for (auto flow : rule_ref->flows) {
      assert(parent_flow != flow);
      assert(subset(flow->ip_prefix, rule_ref->ip_prefix));
      auto data_iter = flow->data.find(source);
      assert(data_iter != flow->data.end());
      assert(data_iter->second == rule_ref);
      data_iter->second = parent_rule_ref;
      auto flow_result = parent_rule_ref->flows.insert(flow);
      assert(ok(flow_result));
    }
  }
  insert_flows(affected_flows, rule_ref->flows);
  m_rule_set.erase(rule_ref);
  return true;
}

} // namespace nopticon
