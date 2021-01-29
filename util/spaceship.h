#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#if defined(__clang__) && __clang_major__ < 13

#include <compare>
#include <type_traits>

namespace {

template<typename T>
struct is_pair_helper : std::false_type {};

template<typename A, typename B>
struct is_pair_helper<std::pair<A, B>> : std::true_type {};

template<typename T>
struct is_pair : is_pair_helper<typename std::remove_cv<T>::type> {};


inline
std::weak_ordering operator<=>(const std::string &lhs, const std::string &rhs) {
  auto cmp = lhs.compare(rhs);
  if (cmp == 0)
    return std::weak_ordering::equivalent;
  return cmp < 0 ? std::weak_ordering::less : std::weak_ordering::equivalent;
}

template <typename T, bool >
std::weak_ordering compare_iterators(T &&I, const T &E, T &&II, const T &EE);

template <typename T,
    std::enable_if_t<!is_pair<typename T::value_type>::value, bool> = true>
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
std::weak_ordering compare_pair(const std::pair<X,Y> &lhs,
                                const std::pair<X,Y> &rhs) {
  auto cmp1 = lhs.first <=> rhs.first;
  if (std::is_neq(cmp1))
    return cmp1;
  return lhs.second <=> rhs.second;
}

template <typename T,
    std::enable_if_t<is_pair<typename T::value_type>::value, bool> = true>
std::weak_ordering compare_iterators(T &&I, const T &E, T &&II, const T &EE) {
  while (I != E && II != EE) {
    auto cmp = compare_pair(*I, *II);
    if (std::is_neq(cmp))
      return cmp;
    ++I, ++II;
  }
  if (I == E)
    return II == EE ? std::weak_ordering::equivalent : std::weak_ordering::less;
  return std::weak_ordering::greater;
}

template <typename T,
    std::enable_if_t<!is_pair<typename T::value_type>::value, bool> = true>
std::weak_ordering compare_iterators(T &&I, const T &E, T &&II, const T &EE) {
  while (I != E && II != EE) {
    auto cmp = compare_pair(*I, *II);
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
