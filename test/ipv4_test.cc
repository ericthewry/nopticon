// Copyright 2018 Alex Horn. All rights reserved.
// Use of this source code is governed by a LICENSE.

#include "ipv4_test_data.hh"

#include <algorithm>
#include <iostream>
#include <ip_prefix_tree.hh>

using namespace nopticon;

static void test_validity() {
  assert(not ip_prefix_t(13UL << 3, 28U).is_valid());
  assert(not ip_prefix_t(13UL << 3, 27U).is_valid());
  assert(not ip_prefix_t(1UL << 3, 28U).is_valid());
}

std::ostream &operator<<(std::ostream &stream, const ip_range_t &i) {
  return stream << "[" << i.low << ":" << i.high << "]";
}

static void test_ip_prefix_length() {
  assert(ip_prefix_length(ip_prefix_0_255) == 24U);
  assert(ip_prefix_length(ip_prefix_64_127) == 26U);
  assert(ip_prefix_length(ip_prefix_0_0) == 0U);
  assert(ip_prefix_length(ip_prefix_n_32) == 32U);
}

static void test_overlap_and_subset() {
  assert(subset(ip_prefix_0_15, ip_prefix_0_15));
  assert(subset(ip_prefix_0_15, ip_prefix_0_0));
  assert(subset(ip_prefix_n_32, ip_prefix_0_255));

  for (auto &x : ip_prefix_vec) {
    assert(x.is_valid());
  }

  bool expect_overlaps, expect_subset;
  for (auto &x : ip_prefix_vec) {
    ip_range_t i{x};
    for (auto &y : ip_prefix_vec) {
      ip_range_t j{y};
      expect_overlaps = overlaps(i, j);
      expect_subset = subset(i, j);
      assert(overlaps(x, y) == expect_overlaps);
      assert(overlaps(y, x) == expect_overlaps);
      assert(subset(x, y) == expect_subset);
      assert(x == y or not overlaps(x, y) or subset(y, x) != expect_subset);
    }
  }
}

// For example, the corpus of IPv4 prefixes which are subsets of
// 0.0.0.0/24 take on the following tree shape:
//
//                   [0:255]
//                  /   |   \
//                 /    |    \
//                /     |     \
//          [0:15]   [64:127]  [128:143]
//          / \        /   \
//         /   \      /     \
//      [2:3] [4:7] [64:79] [96:127]
//                            \
//                           [96:111]
//
// The `ip_prefix_order_t` corresponds to a DFS search.
static void test_ip_prefix_map() {
  assert(std::is_sorted(ip_prefix_vec.begin(), ip_prefix_vec.end(),
                        ip_prefix_order_t()));

  ip_prefix_map_t<unsigned> ip_prefix_map;
  for (unsigned i = 0; i < ip_prefix_vec.size(); ++i) {
    ip_prefix_map[ip_prefix_vec[i]] = i;
  }

  auto ip_prefix_map_iter = ip_prefix_map.begin();
  for (const auto &ip_prefix : ip_prefix_vec) {
    assert(ip_prefix_map_iter != ip_prefix_map.end());
    assert(ip_prefix == ip_prefix_map_iter->first);
    ++ip_prefix_map_iter;
  }
}

static void test_disjoint_ranges() {
  assert(std::is_sorted(ip_prefix_vec.begin(), ip_prefix_vec.end(),
                        ip_prefix_order_t()));

  ip_prefix_tree_t<int> ip_prefix_tree;
  ip_prefix_tree_t<int>::id_t next_id = 0;
  for (auto &ip_prefix : {ip_prefix_2_3, ip_prefix_0_255, ip_prefix_0_15,
                          ip_prefix_4_7, ip_prefix_64_79, ip_prefix_64_127}) {
    ip_prefix_tree_t<int>::ptr_t parent = nullptr;
    auto &ip_prefix_tree_ref =
        ip_prefix_tree.insert(ip_prefix, next_id, parent);
    assert(ip_prefix_tree_ref.id == next_id);
    ++next_id;
    assert(parent != nullptr);
  }
  {
    auto ip_ranges = disjoint_ranges(ip_prefix_tree.find(ip_prefix_0_15));
    assert(ip_ranges == ip_range_vec_t({{0, 1}, {8, 14}}));
  }
  {
    auto ip_ranges = disjoint_ranges(ip_prefix_tree.find(ip_prefix_64_127));
    assert(ip_ranges == ip_range_vec_t({{80, 126}}));
  }
  {
    ip_prefix_tree_t<int>::ptr_t parent = nullptr;
    auto &ip_prefix_tree_ref =
        ip_prefix_tree.insert(ip_prefix_96_127, next_id, parent);
    assert(ip_prefix_tree_ref.id == next_id);
    ++next_id;
    assert(parent != nullptr);
  }
  {
    auto ip_ranges = disjoint_ranges(ip_prefix_tree.find(ip_prefix_64_127));
    assert(ip_ranges == ip_range_vec_t({{80, 95}}));
  }
}

static void test_ip_prefix_tree_with_subset(const ip_prefix_t &x,
                                            const ip_prefix_t &y) {
  assert(subset(x, y));
  bool has_next;
  ip_prefix_tree_t<int>::ptr_t parent;
  {
    ip_prefix_tree_t<int> ip_prefix_tree;
    parent = nullptr;
    auto &ip_prefix_tree_0 = ip_prefix_tree.insert(x, 0, parent);
    assert(ip_prefix_tree_0.id == 0);
    assert(parent == &ip_prefix_tree);
    assert(ip_prefix_tree.find(x) == &ip_prefix_tree_0);
    assert(ip_prefix_tree.find(y) == nullptr);
    auto iter = ip_prefix_tree_0.iter();
    has_next = iter.next();
    assert(not has_next);
    parent = nullptr;
    auto &ip_prefix_tree_1 = ip_prefix_tree.insert(y, 1, parent);
    assert(ip_prefix_tree_1.id == 1);
    assert(parent == &ip_prefix_tree);
    assert(ip_prefix_tree.find(x) == &ip_prefix_tree_0);
    assert(ip_prefix_tree.find(y) == &ip_prefix_tree_1);
    assert(ip_prefix_tree.children().size() == 1);
    assert(ip_prefix_tree_1.children().size() == 1);
    assert(ip_prefix_tree_0.children().empty());
    iter = ip_prefix_tree_1.iter();
    assert(ip_prefix_tree_1 == *iter);
    has_next = iter.next();
    assert(has_next);
    assert(ip_prefix_tree_0 == *iter);
    has_next = iter.next();
    assert(not has_next);
  }

  {
    ip_prefix_tree_t<int> ip_prefix_tree;
    parent = nullptr;
    auto &ip_prefix_tree_0 = ip_prefix_tree.insert(y, 0, parent);
    assert(ip_prefix_tree_0.id == 0);
    assert(parent == &ip_prefix_tree);
    assert(ip_prefix_tree.find(y) == &ip_prefix_tree_0);
    assert(ip_prefix_tree.find(x) == nullptr);
    auto iter = ip_prefix_tree_0.iter();
    has_next = iter.next();
    assert(not has_next);
    parent = nullptr;
    auto &ip_prefix_tree_1 = ip_prefix_tree.insert(x, 1, parent);
    assert(ip_prefix_tree_1.id == 1);
    assert(parent == &ip_prefix_tree_0);
    assert(ip_prefix_tree.find(y) == &ip_prefix_tree_0);
    assert(ip_prefix_tree.find(x) == &ip_prefix_tree_1);
    assert(ip_prefix_tree.children().size() == 1);
    assert(ip_prefix_tree_0.children().size() == 1);
    assert(ip_prefix_tree_1.children().empty());
    iter = ip_prefix_tree_1.iter();
    assert(ip_prefix_tree_1 == *iter);
    has_next = iter.next();
    assert(not has_next);
    iter = ip_prefix_tree_0.iter();
    assert(ip_prefix_tree_0 == *iter);
    has_next = iter.next();
    assert(has_next);
    assert(ip_prefix_tree_1 == *iter);
    has_next = iter.next();
    assert(not has_next);
  }
}

static void test_empty_ip_prefix_tree() {
  assert(ip_prefix_0_15.mask == ip_prefix_0_7.mask + ip_prefix_8_15.mask + 1UL);
  ip_prefix_tree_t<int>::ptr_t parent;
  {
    ip_prefix_tree_t<int> ip_prefix_tree;
    parent = nullptr;
    auto &ip_prefix_tree_0 = ip_prefix_tree.insert(ip_prefix_0_15, 0, parent);
    assert(ip_prefix_tree_0.id == 0);
    assert(parent == &ip_prefix_tree);
    assert(not ip_prefix_tree.is_empty());

    parent = nullptr;
    auto &ip_prefix_tree_1 = ip_prefix_tree.insert(ip_prefix_0_7, 1, parent);
    assert(ip_prefix_tree_1.id == 1);
    assert(parent == &ip_prefix_tree_0);
    assert(not ip_prefix_tree_0.is_empty());
    assert(not ip_prefix_tree_1.is_empty());

    parent = nullptr;
    auto &ip_prefix_tree_2 = ip_prefix_tree.insert(ip_prefix_8_15, 2, parent);
    assert(ip_prefix_tree_2.id == 2);
    assert(parent == &ip_prefix_tree_0);
    assert(ip_prefix_tree_0.is_empty());
    assert(not ip_prefix_tree_1.is_empty());
    assert(not ip_prefix_tree_2.is_empty());
  }
  {
    parent = nullptr;
    ip_prefix_tree_t<int> ip_prefix_tree;
    auto &ip_prefix_tree_0 = ip_prefix_tree.insert(ip_prefix_0_15, 0, parent);
    assert(ip_prefix_tree_0.id == 0);
    assert(parent == &ip_prefix_tree);
    assert(not ip_prefix_tree.is_empty());

    parent = nullptr;
    auto &ip_prefix_tree_1 = ip_prefix_tree.insert(ip_prefix_8_15, 1, parent);
    assert(ip_prefix_tree_1.id == 1);
    assert(parent == &ip_prefix_tree_0);
    assert(not ip_prefix_tree_0.is_empty());
    assert(not ip_prefix_tree_1.is_empty());

    parent = nullptr;
    auto &ip_prefix_tree_2 = ip_prefix_tree.insert(ip_prefix_0_7, 2, parent);
    assert(ip_prefix_tree_2.id == 2);
    assert(parent == &ip_prefix_tree_0);
    assert(ip_prefix_tree_0.is_empty());
    assert(not ip_prefix_tree_1.is_empty());
    assert(not ip_prefix_tree_2.is_empty());
  }
  {
    parent = nullptr;
    ip_prefix_tree_t<int> ip_prefix_tree;
    auto &ip_prefix_tree_0 = ip_prefix_tree.insert(ip_prefix_0_7, 0, parent);
    assert(ip_prefix_tree_0.id == 0);
    assert(parent == &ip_prefix_tree);
    assert(not ip_prefix_tree.is_empty());

    parent = nullptr;
    auto &ip_prefix_tree_1 = ip_prefix_tree.insert(ip_prefix_0_15, 1, parent);
    assert(ip_prefix_tree_1.id == 1);
    assert(parent == &ip_prefix_tree);
    assert(not ip_prefix_tree_0.is_empty());
    assert(not ip_prefix_tree_1.is_empty());

    parent = nullptr;
    auto &ip_prefix_tree_2 = ip_prefix_tree.insert(ip_prefix_8_15, 2, parent);
    assert(ip_prefix_tree_2.id == 2);
    assert(parent == &ip_prefix_tree_1);
    assert(not ip_prefix_tree_0.is_empty());
    assert(ip_prefix_tree_1.is_empty());
    assert(not ip_prefix_tree_2.is_empty());
  }
  {
    parent = nullptr;
    ip_prefix_tree_t<int> ip_prefix_tree;
    auto &ip_prefix_tree_0 = ip_prefix_tree.insert(ip_prefix_8_15, 0, parent);
    assert(ip_prefix_tree_0.id == 0);
    assert(parent == &ip_prefix_tree);
    assert(not ip_prefix_tree.is_empty());

    parent = nullptr;
    auto &ip_prefix_tree_1 = ip_prefix_tree.insert(ip_prefix_0_15, 1, parent);
    assert(ip_prefix_tree_1.id == 1);
    assert(parent == &ip_prefix_tree);
    assert(not ip_prefix_tree_0.is_empty());
    assert(not ip_prefix_tree_1.is_empty());

    parent = nullptr;
    auto &ip_prefix_tree_2 = ip_prefix_tree.insert(ip_prefix_0_7, 2, parent);
    assert(ip_prefix_tree_2.id == 2);
    assert(parent == &ip_prefix_tree_1);
    assert(not ip_prefix_tree_0.is_empty());
    assert(ip_prefix_tree_1.is_empty());
    assert(not ip_prefix_tree_2.is_empty());
  }
  {
    parent = nullptr;
    ip_prefix_tree_t<int> ip_prefix_tree;
    auto &ip_prefix_tree_0 = ip_prefix_tree.insert(ip_prefix_0_7, 0, parent);
    assert(ip_prefix_tree_0.id == 0);
    assert(parent == &ip_prefix_tree);
    assert(not ip_prefix_tree.is_empty());

    parent = nullptr;
    auto &ip_prefix_tree_1 = ip_prefix_tree.insert(ip_prefix_8_15, 1, parent);
    assert(ip_prefix_tree_1.id == 1);
    assert(parent == &ip_prefix_tree);
    assert(not ip_prefix_tree_0.is_empty());
    assert(not ip_prefix_tree_1.is_empty());

    parent = nullptr;
    auto &ip_prefix_tree_2 = ip_prefix_tree.insert(ip_prefix_0_15, 2, parent);
    assert(ip_prefix_tree_2.id == 2);
    assert(parent == &ip_prefix_tree);
    assert(not ip_prefix_tree_0.is_empty());
    assert(not ip_prefix_tree_1.is_empty());
    assert(ip_prefix_tree_2.is_empty());
  }
  {
    ip_prefix_tree_t<int> ip_prefix_tree;
    parent = nullptr;
    auto &ip_prefix_tree_0 = ip_prefix_tree.insert(ip_prefix_8_15, 0, parent);
    assert(ip_prefix_tree_0.id == 0);
    assert(parent == &ip_prefix_tree);
    assert(not ip_prefix_tree.is_empty());

    parent = nullptr;
    auto &ip_prefix_tree_1 = ip_prefix_tree.insert(ip_prefix_0_7, 1, parent);
    assert(ip_prefix_tree_1.id == 1);
    assert(parent == &ip_prefix_tree);
    assert(not ip_prefix_tree_0.is_empty());
    assert(not ip_prefix_tree_1.is_empty());

    parent = nullptr;
    auto &ip_prefix_tree_2 = ip_prefix_tree.insert(ip_prefix_0_15, 2, parent);
    assert(ip_prefix_tree_2.id == 2);
    assert(parent == &ip_prefix_tree);
    assert(not ip_prefix_tree_0.is_empty());
    assert(not ip_prefix_tree_1.is_empty());
    assert(ip_prefix_tree_2.is_empty());
  }
}

static void test_ip_prefix_tree() {
  ip_prefix_vec_t ip_prefix_perm_vec = {
      ip_prefix_0_255,  ip_prefix_0_15,  ip_prefix_64_127, ip_prefix_128_143,
      ip_prefix_2_3,    ip_prefix_4_7,   ip_prefix_8_15,   ip_prefix_64_79,
      ip_prefix_96_127, ip_prefix_96_111};

  ip_prefix_tree_t<int>::id_t next_id = 0;
  ip_prefix_tree_t<int>::ptr_t parent;
  ip_prefix_tree_t<int> expect_ip_prefix_tree;
  std::vector<ip_prefix_tree_ptr_t<int>> parents;
  for (auto &x : ip_prefix_perm_vec) {
    parent = nullptr;
    auto &expect_ip_prefix_tree_ref =
        expect_ip_prefix_tree.insert(x, next_id, parent);
    assert(expect_ip_prefix_tree_ref.id == next_id);
    ++next_id;
    assert(parent != nullptr);
    assert(expect_ip_prefix_tree.find(x, parents) ==
           &expect_ip_prefix_tree_ref);
    ip_prefix_t parent_ip_prefix;
    for (auto parent : parents) {
      assert(subset(parent->ip_prefix, parent_ip_prefix));
      parent_ip_prefix = parent->ip_prefix;
    }
    assert((&expect_ip_prefix_tree_ref == &expect_ip_prefix_tree) ==
           parents.empty());
    parents.clear();
  }

  auto expect_ip_prefix_tree_iter = expect_ip_prefix_tree.iter();
  assert(expect_ip_prefix_tree_iter->ip_prefix == ip_prefix_0_0);
  {
    std::size_t i = 0;
    while (expect_ip_prefix_tree_iter.next()) {
      assert(i < ip_prefix_perm_vec.size());
      assert(expect_ip_prefix_tree_iter->id == i);
      assert(expect_ip_prefix_tree_iter->ip_prefix == ip_prefix_perm_vec[i++]);
    }
    assert(i == ip_prefix_perm_vec.size());
  }
  assert(next_id != 0);
  {
    ip_prefix_order_t ip_prefix_order;
    do {
      ip_prefix_tree_t<int> actual_ip_prefix_tree;
      for (auto &x : ip_prefix_perm_vec) {
        parent = nullptr;
        auto &ip_prefix_tree_ref =
            actual_ip_prefix_tree.insert(x, next_id, parent);
        assert(ip_prefix_tree_ref.id == next_id);
        ++next_id;
        assert(parent != nullptr);
      }
      auto expect_ip_prefix_tree_iter = expect_ip_prefix_tree.iter();
      auto actual_ip_prefix_tree_iter = actual_ip_prefix_tree.iter();
      bool expect_has_next, actual_has_next;
      do {
        assert(expect_ip_prefix_tree_iter->ip_prefix ==
               actual_ip_prefix_tree_iter->ip_prefix);
        assert(expect_ip_prefix_tree_iter->is_empty() ==
               actual_ip_prefix_tree_iter->is_empty());
        expect_has_next = expect_ip_prefix_tree_iter.next();
        actual_has_next = actual_ip_prefix_tree_iter.next();
      } while (expect_has_next and actual_has_next);
      assert(not expect_has_next);
      assert(not actual_has_next);
    } while (std::next_permutation(ip_prefix_perm_vec.begin(),
                                   ip_prefix_perm_vec.end(), ip_prefix_order));
  }
}

void run_ipv4_test() {
  test_ip_prefix_length();
  test_validity();
  test_overlap_and_subset();
  test_ip_prefix_map();
  test_ip_prefix_tree_with_subset(ip_prefix_2_3, ip_prefix_0_15);
  test_ip_prefix_tree_with_subset(ip_prefix_197_dot_157_slash_19,
                                  ip_prefix_197_dot_157_slash_18);
  test_ip_prefix_tree();
  test_empty_ip_prefix_tree();
  test_disjoint_ranges();
}
