// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "util/compiler.h"
#include <algorithm>
#include <bit>
#include <cctype>

#ifdef _MSC_VER
#include <intrin.h>
#endif

using namespace std;

#if defined(__clang__) && __clang_major__ < 13 && defined(__apple_build_version__)
namespace {
bool has_single_bit(uint64_t n) {
  return n != 0 && (n & (n - 1)) == 0;
}

unsigned bit_width(uint64_t n) {
  return 64 - countl_zero(n);
}
}
#endif

namespace util {

unsigned ilog2(uint64_t n) {
  return n == 0 ? 0 : bit_width(n) - 1;
}

unsigned ilog2_ceil(uint64_t n, bool up_power2) {
  auto log = ilog2(n);
  return !up_power2 && is_power2(n) ? log : log + 1;
}

bool is_power2(uint64_t n, uint64_t *log) {
  if (!has_single_bit(n))
    return false;

  if (log)
    *log = ilog2(n);
  return true;
}

unsigned num_sign_bits(uint64_t n) {
  return max(countl_zero(n), countl_one(n)) -1;
}

#ifdef _MSC_VER
uint64_t add_saturate(uint64_t a, uint64_t b) {
  unsigned __int64 res;
  static_assert(sizeof(res) == sizeof(uint64_t));
  return _addcarry_u64(0, a, b, &res) ? UINT64_MAX : res;
}

uint64_t mul_saturate(uint64_t a, uint64_t b) {
  return __umulh(a, b) ? UINT64_MAX : a * b;
}
#else
uint64_t add_saturate(uint64_t a, uint64_t b) {
  unsigned long long res;
  static_assert(sizeof(res) == sizeof(uint64_t));
  return __builtin_uaddll_overflow(a, b, &res) ? UINT64_MAX : res;
}

uint64_t mul_saturate(uint64_t a, uint64_t b) {
  unsigned long long res;
  static_assert(sizeof(res) == sizeof(uint64_t));
  return __builtin_umulll_overflow(a, b, &res) ? UINT64_MAX : res;
}
#endif

uint64_t divide_up(uint64_t n, uint64_t amount) {
  return (n + amount - 1) / amount;
}

uint64_t round_up(uint64_t n, uint64_t amount) {
  return divide_up(n, amount) * amount;
}

bool stricontains(const string_view &needle, const string_view &haystack) {
    return search(
      needle.begin(), needle.end(),haystack.begin(), haystack.end(),
      [](char ch1, char ch2) { return tolower(ch1) == tolower(ch2); }
    ) != needle.end();
}

}
