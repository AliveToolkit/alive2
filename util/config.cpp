// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "util/config.h"
#include <iostream>

using namespace std;

static ostream *debug_os = &cerr;

namespace util::config {

bool symexec_print_each_value = false;
bool skip_smt = false;
bool io_nobuiltin = false;
bool disable_poison_input = false;
bool disable_undef_input = false;
bool debug = false;

ostream &dbg() {
  return *debug_os;
}

void set_debug(ostream &os) {
  debug_os = &os;
}

}
