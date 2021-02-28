#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include <string>
#include <ostream>

namespace util::config {

extern bool symexec_print_each_value;

extern bool skip_smt;

// don't dumo if empty
extern std::string smt_benchmark_dir;

extern bool disable_poison_input;

extern bool disable_undef_input;

extern bool debug;

extern unsigned src_unroll_cnt;

extern unsigned tgt_unroll_cnt;

// The maximum number of bits to use for offset computations. Note that this may
// impact correctness, if values involved in offset computations exceed the
// maximum.
extern unsigned max_offset_bits;

std::ostream &dbg();
void set_debug(std::ostream &os);

}
