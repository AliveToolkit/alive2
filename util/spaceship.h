#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#if defined(__clang__) && __clang_major__ < 15 && defined(__APPLE__)

#include <compare>

namespace std {
inline bool is_neq(std::weak_ordering o) { return o != 0; }
}

namespace {

inline
std::weak_ordering operator<=>(const std::string &lhs, const std::string &rhs) {
  auto cmp = lhs.compare(rhs);
  if (cmp == 0)
    return std::weak_ordering::equivalent;
  return cmp < 0 ? std::weak_ordering::less : std::weak_ordering::greater;
}

template <typename T>
std::weak_ordering compare_iterators(T &&I, const T &E, T &&II, const T &EE);

template <typename T>
std::weak_ordering operator<=>(const std::vector<T> &lhs,
                               const std::vector<T> &rhs) {
  return compare_iterators(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template <typename T>
std::weak_ordering operator<=>(const std::set<T> &lhs, const std::set<T> &rhs) {
  return compare_iterators(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template <typename K, typename V>
std::weak_ordering operator<=>(const std::map<K,V> &lhs,
                               const std::map<K,V> &rhs) {
  return compare_iterators(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template <typename X, typename Y>
std::weak_ordering operator<=>(const std::pair<X,Y> &lhs,
                               const std::pair<X,Y> &rhs) {
  if (auto cmp = lhs.first <=> rhs.first;
      std::is_neq(cmp))
    return cmp;
  return lhs.second <=> rhs.second;
}

template <typename T>
std::weak_ordering compare_iterators(T &&I, const T &E, T &&II, const T &EE) {
  while (I != E && II != EE) {
    auto cmp = *I <=> *II;
    if (std::is_neq(cmp))
      return cmp;
    ++I, ++II;
  }
  if (I == E)
    return II == EE ? std::weak_ordering::equivalent : std::weak_ordering::less;
  return std::weak_ordering::greater;
}

}

#endif
