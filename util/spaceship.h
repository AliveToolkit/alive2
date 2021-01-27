#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#if defined(__clang__) && __clang_major__ < 13

#include <compare>

namespace {

template <typename T>
std::weak_ordering operator<=>(const std::vector<T> &lhs,
                               const std::vector<T> &rhs) {
  auto sz = lhs.size() <=> rhs.size();
  if (std::is_neq(sz))
    return sz;

  auto I = rhs.begin();
  for (const auto &e : lhs) {
    auto cmp = e <=> *I++;
    if (std::is_neq(cmp))
      return cmp;
  }
  return std::weak_ordering::equivalent;
}

template <typename K, typename V>
std::weak_ordering operator<=>(const std::map<K,V> &lhs,
                               const std::map<K,V> &rhs) {
  auto sz = lhs.size() <=> rhs.size();
  if (std::is_neq(sz))
    return sz;

  auto I = rhs.begin();
  for (const auto &[k,v] : lhs) {
    if (auto cmp = k <=> I->first;
        std::is_neq(cmp))
      return cmp;
    if (auto cmp = v <=> I->second;
        std::is_neq(cmp))
      return cmp;
    ++I;
  }
  return std::weak_ordering::equivalent;
}

}

#endif
