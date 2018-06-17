// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "util/compiler.h"
#ifdef _MSC_VER
# include <intrin.h>
#endif

namespace util {

unsigned ilog2(uint64_t n) {
#ifdef __GNUC__
  return n == 0 ? 0 : (64 - __builtin_clzll(n));
#elif defined(_MSC_VER)
# ifdef _M_X64
  return 64 - (unsigned)__lzcnt64(n);
# else
# error Unknown compiler
# endif
#else
# error Unknown compiler
#endif
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

}
