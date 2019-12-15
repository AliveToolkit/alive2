// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "util/compiler.h"
#ifdef _MSC_VER
# include <intrin.h>
#endif

namespace util {

unsigned ilog2(uint64_t n) {
#ifdef __GNUC__
  return n == 0 ? 0 : (63 - __builtin_clzll(n));
#elif defined(_MSC_VER) && defined(_M_X64)
  return 63 - (unsigned)__lzcnt64(n);
#else
# error Unknown compiler
#endif
}

unsigned ilog2_ceil(uint64_t n) {
  auto log = ilog2(n);
  return is_power2(n) ? log : log + 1;
}

bool is_power2(uint64_t n, uint64_t *log) {
  if (n == 0 || (n & (n - 1)) != 0)
    return false;

  if (log)
    *log = ilog2(n);
  return true;
}

unsigned num_sign_bits(uint64_t n) {
#ifdef __clang__
  if (n == 0 || n == -1)
    return 63;
  int zeros = __builtin_clzll(n) - 1;
  int ones = __builtin_clzll(~n) - 1;
  return zeros > ones ? zeros : ones;
#elif defined(__GNUC__)
  return __builtin_clrsbll(n);
#else
# error Unknown compiler
#endif
}

unsigned num_leading_zeros(uint64_t n) {
#if defined(__GNUC__)
  return n == 0 ? 64 : __builtin_clzll(n);
#else
# error Unknown compiler
#endif
}

unsigned num_trailing_zeros(uint64_t n) {
#if defined(__GNUC__)
  return n == 0 ? 64 : __builtin_ctzll(n);
#else
# error Unknown compiler
#endif
}

uint64_t divide_up(uint64_t n, uint64_t amount) {
  return (n + amount - 1) / amount;
}

uint64_t gcd(uint64_t n, uint64_t m) {
  assert(n != 0 || m != 0);
  if (n < m) return gcd(m, n);
  else if (m == 0) return n;
  else if (n % m == 0) return m;
  return gcd(m, n % m);
}

}
