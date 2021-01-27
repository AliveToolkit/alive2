#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#if defined(__clang__) && __clang_major__ < 13

#include <compare>

namespace {

template <typename T>
std::weak_ordering operator<=>(const std::vector<T> &lhs,
                               const std::vector<T> &rhs) {
  if (lhs < rhs) return std::weak_ordering::less;
  if (rhs < lhs) return std::weak_ordering::greater;
  return std::weak_ordering::equivalent;
}

}

#endif
