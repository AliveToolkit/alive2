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

#define sizeof_array(a) (sizeof(a)/sizeof(*a))

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
// if up_power2 is true, then we do +1 for powers of 2
// e.g. ilog2_ceil(8, false) = 3 ; ilog2_ceil(8, true) = 4
unsigned ilog2_ceil(uint64_t n, bool up_power2);
bool is_power2(uint64_t n, uint64_t *log = nullptr);
unsigned num_sign_bits(uint64_t n);

uint64_t add_saturate(uint64_t a, uint64_t b);
uint64_t mul_saturate(uint64_t a, uint64_t b);

uint64_t divide_up(uint64_t n, uint64_t amount); // division with ceiling
uint64_t round_up(uint64_t n, uint64_t amount);

}
