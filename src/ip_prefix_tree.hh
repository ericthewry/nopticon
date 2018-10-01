// Copyright 2018 Alex Horn. All rights reserved.
// Use of this source code is governed by a LICENSE.

#pragma once

#include "ipv4.hh"

#include <deque>
#include <iterator>
#include <utility>

namespace nopticon {

template <class T> inline bool ok(const std::pair<T, bool> &result) {
  return result.second;
}

template <class T> class ip_prefix_tree_t;

template <class T> using ip_prefix_tree_ptr_t = ip_prefix_tree_t<T> *;

template <class T>
using ip_prefix_tree_const_ptr_t = const ip_prefix_tree_t<T> *;

template <class T> class ip_prefix_tree_iter_t;
template <class T> class ip_prefix_tree_const_iter_t;

template <class T> class ip_prefix_tree_t {
public:
  typedef ip_addr_t id_t;
  typedef ip_prefix_tree_iter_t<T> iter_t;
  typedef ip_prefix_tree_ptr_t<T> ptr_t;
  typedef ip_prefix_tree_const_ptr_t<T> const_ptr_t;
  typedef ip_prefix_tree_const_iter_t<T> const_iter_t;

  T data;
  id_t id;
  const ip_prefix_t ip_prefix;

  ip_prefix_tree_t() : id{0}, ip_prefix{} {};
  ip_prefix_tree_t(id_t id, const ip_prefix_t &ip_prefix)
      : id{id}, ip_prefix{ip_prefix} {};

  ~ip_prefix_tree_t() {
    for (auto pair : m_children) {
      assert(pair.second != nullptr);
      delete pair.second;
    }
  }
  ip_prefix_tree_t(ip_prefix_tree_t &&) = delete;
  ip_prefix_tree_t(const ip_prefix_tree_t &) = delete;

  const ip_prefix_map_t<ptr_t> &children() const noexcept { return m_children; }
  const_ptr_t find(const ip_prefix_t &) const;

  ptr_t find(const ip_prefix_t &, std::vector<ptr_t> &);
  ip_prefix_tree_t<T> &insert(const ip_prefix_t &, id_t, ptr_t &);

  iter_t iter() noexcept;
  const_iter_t iter() const noexcept;
  bool is_empty() const noexcept {
    return not m_children.empty() and m_cardinality == m_children.size() - 1;
  }

private:
  friend class ip_prefix_tree_iter_t<T>;
  friend class ip_prefix_tree_const_iter_t<T>;
  ip_prefix_map_t<ptr_t> m_children;
  ip_addr_t m_cardinality = ip_prefix.mask;
};

template <class T>
inline bool operator==(const ip_prefix_tree_t<T> &x,
                       const ip_prefix_tree_t<T> &y) noexcept {
  return &x == &y;
}

template <class T>
ip_range_vec_t disjoint_ranges(ip_prefix_tree_const_ptr_t<T> parent_ptr) {
  assert(parent_ptr != nullptr);
  if (parent_ptr->is_empty()) {
    return {};
  }
  const ip_range_t parent_ip_range{parent_ptr->ip_prefix};
  if (parent_ptr->children().empty()) {
    return {{parent_ip_range.low, parent_ip_range.high}};
  }

  ip_range_vec_t ip_range_vec;
  ip_addr_t high = 0;
  auto child_iter = parent_ptr->children().begin();
  assert(child_iter != parent_ptr->children().end());
  assert(child_iter->second->ip_prefix == child_iter->first);
  assert(child_iter->first != parent_ptr->ip_prefix);
  {
    ip_range_t first_child_ip_range{child_iter->first};
    assert(first_child_ip_range != parent_ip_range);
    if (parent_ip_range.low != first_child_ip_range.low) {
      assert(0 < first_child_ip_range.low);
      ip_range_vec.emplace_back(parent_ip_range.low,
                                first_child_ip_range.low - 1);
    }
    high = first_child_ip_range.high + 1;
  }
  assert(high != 0);
  ++child_iter;
  for (; child_iter != parent_ptr->children().end(); ++child_iter) {
    ip_range_t child_ip_range{child_iter->first};
    assert(high <= child_ip_range.low);
    if (high != child_ip_range.low) {
      assert(0 < child_ip_range.low);
      ip_range_vec.emplace_back(high, child_ip_range.low - 1);
    }
    high = child_ip_range.high + 1;
  }
  if (high != 0 and high < parent_ip_range.high) {
    ip_range_vec.emplace_back(high, parent_ip_range.high - 1);
  }
  return ip_range_vec;
}

/// Breadth-first
template <class T> class ip_prefix_tree_iter_t {
public:
  ip_prefix_tree_iter_t(ip_prefix_tree_ptr_t<T> start) : m_queue({start}) {}

  bool next() {
    assert(not m_queue.empty());
    auto ip_prefix_tree_ptr = m_queue.front();
    m_queue.pop_front();
    for (auto pair : ip_prefix_tree_ptr->m_children) {
      m_queue.push_back(pair.second);
    }
    return not m_queue.empty();
  }

  ip_prefix_tree_t<T> &operator*() const {
    assert(not m_queue.empty());
    return *m_queue.front();
  }

  ip_prefix_tree_ptr_t<T> operator->() const {
    assert(not m_queue.empty());
    return m_queue.front();
  }

  ip_prefix_tree_ptr_t<T> ptr() const {
    assert(not m_queue.empty());
    return m_queue.front();
  }

private:
  std::deque<ip_prefix_tree_ptr_t<T>> m_queue;
};

/// Breadth-first
template <class T> class ip_prefix_tree_const_iter_t {
public:
  ip_prefix_tree_const_iter_t(ip_prefix_tree_const_ptr_t<T> start)
      : m_queue({start}) {}

  bool next() {
    assert(not m_queue.empty());
    auto ip_prefix_tree_ptr = m_queue.front();
    m_queue.pop_front();
    for (auto pair : ip_prefix_tree_ptr->m_children) {
      m_queue.push_back(pair.second);
    }
    return not m_queue.empty();
  }

  const ip_prefix_tree_t<T> &operator*() const {
    assert(not m_queue.empty());
    return *m_queue.front();
  }

  ip_prefix_tree_const_ptr_t<T> operator->() const {
    assert(not m_queue.empty());
    return m_queue.front();
  }

  ip_prefix_tree_const_ptr_t<T> ptr() const {
    assert(not m_queue.empty());
    return m_queue.front();
  }

private:
  std::deque<ip_prefix_tree_const_ptr_t<T>> m_queue;
};

template <class T>
ip_prefix_tree_const_ptr_t<T>
ip_prefix_tree_t<T>::find(const ip_prefix_t &ip_prefix) const {
  if (this->ip_prefix == ip_prefix) {
    return this;
  }

  auto ip_prefix_tree_ptr = this;
  for (;;) {
    assert(subset(ip_prefix, ip_prefix_tree_ptr->ip_prefix));
    auto &children = ip_prefix_tree_ptr->m_children;
    auto child_iter = children.lower_bound(ip_prefix);

    if (child_iter != children.end() and child_iter->first == ip_prefix) {
      return child_iter->second;
    }

    // By ip_prefix_order_t, there are two cases.
    //
    // 1. Both IP prefixex are disjoint:
    //    [--ip_prefix--] [--child_iter->ip_prefix--]
    //
    // 2. Or subset(child_iter->first, ip_prefix):
    //     [-----------ip_prefix-----------]
    //        [--child_iter->ip_prefix--]
    assert(child_iter == children.end() or
           not subset(ip_prefix, child_iter->first));

    if (child_iter == children.begin()) {
      return nullptr;
    }

    child_iter = std::prev(child_iter);
    if (subset(ip_prefix, child_iter->first)) {
      ip_prefix_tree_ptr = child_iter->second;
    } else {
      return nullptr;
    }
  }

  return nullptr;
}

template <class T>
ip_prefix_tree_ptr_t<T>
ip_prefix_tree_t<T>::find(const ip_prefix_t &ip_prefix,
                          std::vector<ip_prefix_tree_ptr_t<T>> &parents) {
  if (this->ip_prefix == ip_prefix) {
    return this;
  }

  auto ip_prefix_tree_ptr = this;
  for (;;) {
    assert(subset(ip_prefix, ip_prefix_tree_ptr->ip_prefix));
    parents.push_back(ip_prefix_tree_ptr);
    auto &children = ip_prefix_tree_ptr->m_children;
    auto child_iter = children.lower_bound(ip_prefix);

    if (child_iter != children.end() and child_iter->first == ip_prefix) {
      return child_iter->second;
    }

    // By ip_prefix_order_t, there are two cases.
    //
    // 1. Both IP prefixex are disjoint:
    //    [--ip_prefix--] [--child_iter->ip_prefix--]
    //
    // 2. Or subset(child_iter->first, ip_prefix):
    //     [-----------ip_prefix-----------]
    //        [--child_iter->ip_prefix--]
    assert(child_iter == children.end() or
           not subset(ip_prefix, child_iter->first));

    if (child_iter == children.begin()) {
      return nullptr;
    }

    child_iter = std::prev(child_iter);
    if (subset(ip_prefix, child_iter->first)) {
      ip_prefix_tree_ptr = child_iter->second;
    } else {
      return nullptr;
    }
  }

  return nullptr;
}

template <class T>
ip_prefix_tree_t<T> &ip_prefix_tree_t<T>::insert(const ip_prefix_t &ip_prefix,
                                                 id_t next_id, ptr_t &parent) {
  if (this->ip_prefix == ip_prefix) {
    parent = nullptr;
    return *this;
  }

  auto ip_prefix_tree_ptr = this;
  while (not ip_prefix_tree_ptr->m_children.empty()) {
    assert(subset(ip_prefix, ip_prefix_tree_ptr->ip_prefix));
    assert(ip_prefix_tree_ptr->id != next_id);
    auto &children = ip_prefix_tree_ptr->m_children;
    auto child_iter = children.lower_bound(ip_prefix);

    if (child_iter != children.end()) {
      if (child_iter->first == ip_prefix) {
        parent = ip_prefix_tree_ptr;
        return *child_iter->second;
      }

      if (subset(child_iter->first, ip_prefix)) {
        auto new_ip_prefix_tree_ptr =
            new ip_prefix_tree_t<T>(next_id, ip_prefix);
        auto &new_children = new_ip_prefix_tree_ptr->m_children;
        do {
          assert(child_iter->second->id != next_id);
          new_children.emplace(child_iter->first, child_iter->second);
          auto child_cardinality = child_iter->second->m_cardinality;
          assert(new_ip_prefix_tree_ptr->m_cardinality >= child_cardinality);
          new_ip_prefix_tree_ptr->m_cardinality -= child_cardinality;
          child_iter = children.erase(child_iter);
        } while (child_iter != children.end() and
                 subset(child_iter->first, ip_prefix));
        auto emplace_result =
            children.emplace(ip_prefix, new_ip_prefix_tree_ptr);
        assert(ok(emplace_result));
        assert(new_ip_prefix_tree_ptr == emplace_result.first->second);
        parent = ip_prefix_tree_ptr;
        return *new_ip_prefix_tree_ptr;
      }
    }

    assert(child_iter == children.end() or
           not overlaps(child_iter->first, ip_prefix));

    if (child_iter == children.begin()) {
      break;
    }

    child_iter = std::prev(child_iter);
    if (subset(ip_prefix, child_iter->first)) {
      ip_prefix_tree_ptr = child_iter->second;
    } else {
      break;
    }
  }

  assert(ip_prefix_tree_ptr != nullptr);
  assert(ip_prefix_tree_ptr->m_cardinality >= ip_prefix.mask);
  ip_prefix_tree_ptr->m_cardinality -= ip_prefix.mask;
  auto emplace_result = ip_prefix_tree_ptr->m_children.emplace(
      ip_prefix, new ip_prefix_tree_t<T>(next_id, ip_prefix));
  assert(ok(emplace_result));
  parent = ip_prefix_tree_ptr;
  return *emplace_result.first->second;
}

template <class T>
ip_prefix_tree_iter_t<T> ip_prefix_tree_t<T>::iter() noexcept {
  return {this};
}

template <class T>
ip_prefix_tree_const_iter_t<T> ip_prefix_tree_t<T>::iter() const noexcept {
  return {this};
}

} // namespace nopticon
