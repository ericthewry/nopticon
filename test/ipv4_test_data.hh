// Copyright 2018 Alex Horn. All rights reserved.
// Use of this source code is governed by a LICENSE.

#pragma once

#include <ipv4.hh>

using namespace nopticon;

/// Corpus
extern ip_prefix_t
    // Range of all IPv4 addresses
    ip_prefix_0_0,
    // Single IPv4 address
    ip_prefix_n_32,
    //   range    | length
    // -----------+--------
    //  [0:255]   | 24U
    // -----------+--------
    //  [64:127]  | 26U
    // -----------+--------
    //  [64:79]   | 28U
    // -----------+--------
    //  [96:127]  | 27U
    // -----------+--------
    //  [96:111]  | 28U
    // -----------+--------
    //  [128:143] | 28U
    // -----------+--------
    ip_prefix_0_255, ip_prefix_64_127, ip_prefix_64_79, ip_prefix_96_127,
    ip_prefix_96_111, ip_prefix_128_143,
    // 197.157.0.0/18:  [----------------------------------]
    // 197.157.0.0/19:  [----------------]
    // 197.157.32.0/19:                   [----------------]
    ip_prefix_197_dot_157_slash_18, ip_prefix_197_dot_157_slash_19,
    ip_prefix_197_dot_157_dot_32_slash_19,
    // 2.0.0.0/16:  [---]
    // 2.16.0.0/16:       [------]
    // 2.16.8.0/25:         [--]
    ip_prefix_2_slash_16, ip_prefix_2_dot_16_slash_16,
    ip_prefix_2_dot_16_dot_8_slash_25,
    // 2.16.0.0/13:  [-------------------------------------------------]
    // 2.16.0.0/23:  [-----]
    // 2.17.0.0/20:                      [---------]
    // 2.17.16.0/22:                                [------------]
    ip_prefix_2_dot_16_slash_13, ip_prefix_2_dot_16_slash_23,
    ip_prefix_2_dot_17_slash_20, ip_prefix_2_dot_17_dot_16_slash_22,
    //       0 1 2 3 4 5 6 7 8 9 A B C D E F
    // ****: [-----------------------------]
    // 0***: [-------------]
    // 001*:     [-]
    // 01**:         [-----]
    // 1***:                 [-------------]
    // 10**:                 [-----]
    ip_prefix_0_15, ip_prefix_0_7, ip_prefix_2_3, ip_prefix_4_7, ip_prefix_8_15,
    ip_prefix_8_11,
    // u: [---------------------------------------]
    // v:      [--------]
    // i:                   [-----------------]
    // j:                   [-------]
    ip_prefix_u, ip_prefix_v, ip_prefix_i, ip_prefix_j,
    // w: [------------------------------------]
    // x:    [----------------------------]
    // y:    [-----------]
    // z:                 [----------]
    ip_prefix_w, ip_prefix_x, ip_prefix_y, ip_prefix_z;

typedef std::vector<ip_prefix_t> ip_prefix_vec_t;

/// Sorted by `ip_prefix_order_t`
extern ip_prefix_vec_t ip_prefix_vec;
