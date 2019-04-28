#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include <cassert>
#include <cstdint>

#ifdef _MSC_VER
# define UNREACHABLE() __assume(0)
#elif defined(__GNUC__)
# define UNREACHABLE() __builtin_unreachable()
#else
# error "unknown platform"
#endif

#ifndef NDEBUG
# define ENSURE(x) assert(x)
#else
# define ENSURE(x) (void)(x)
#endif

namespace util {

template <typename T>
struct const_strip_unique_ptr {
  const T &container;
  const_strip_unique_ptr(const T &container) : container(container) {}

  struct const_iterator {
    typename T::const_iterator I;
    const_iterator() {}
    const_iterator(typename T::const_iterator I) : I(I) {}
    const auto& operator*() const { return *I->get(); }
    const_iterator operator++(void) { return ++I; }
    bool operator!=(const const_iterator &other) const { return I != other.I; }
  };

  const_iterator begin() const { return container.begin(); }
  const_iterator end() const   { return container.end(); }
};

unsigned ilog2(uint64_t n);
bool is_power2(uint64_t n, uint64_t *log = nullptr);
unsigned num_sign_bits(uint64_t n);
unsigned num_leading_zeros(uint64_t n);
unsigned num_trailing_zeros(uint64_t n);

}
