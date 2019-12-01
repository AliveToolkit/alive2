#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include <ostream>

namespace util::config {

extern bool symexec_print_each_value;

extern bool skip_smt;

extern bool disable_poison_input;

extern bool disable_undef_input;

extern bool debug;

std::ostream &dbg();
void set_debug(std::ostream &os);

}
