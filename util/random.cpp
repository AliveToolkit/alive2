// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "util/random.h"
#include <random>

using namespace std;

static default_random_engine re;

static void seed() {
  static bool seeded = false;
  if (!seeded) {
    random_device rd;
    re.seed(rd());
    seeded = true;
  }
}

static const char chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

namespace util {

string get_random_str(unsigned num_chars) {
  seed();
  uniform_int_distribution<unsigned> rand(0, sizeof(chars)-2);
  string s;
  for (unsigned i = 0; i < num_chars; ++i) {
    s += chars[rand(re)];
  }
  return s;
}

}
