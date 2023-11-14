#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include <string>
#include <ostream>

namespace util::config {

extern bool symexec_print_each_value;

extern bool skip_smt;

// don't dump if empty
extern std::string smt_benchmark_dir;

extern bool disable_poison_input;

extern bool disable_undef_input;

extern bool tgt_is_asm;

extern bool check_if_src_is_ub;

/// This is a special mode to verify that LLVM's optimizations are not
/// exploiting UB. In particular, we disallow any UB related with arithmetic,
/// but UB related with memory optimizations is OK. UB can only be refined to
/// call @llvm.trap().
extern bool disallow_ub_exploitation;

extern bool debug;

extern unsigned src_unroll_cnt;

extern unsigned tgt_unroll_cnt;

// The maximum number of bits to use for offset computations. Note that this may
// impact correctness, if values involved in offset computations exceed the
// maximum.
extern unsigned max_offset_bits;

// Max bits for size_t. This limits the internal address space, like max block
// size and size of pointers (not to be confused with program pointer size).
extern unsigned max_sizet_bits;

extern bool refine_tgt;

std::ostream &dbg();
void set_debug(std::ostream &os);

}
