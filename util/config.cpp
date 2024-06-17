// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "util/config.h"
#include <iostream>

using namespace std;

static ostream *debug_os = &cerr;

namespace util::config {

bool symexec_print_each_value = false;
bool skip_smt = false;
string smt_benchmark_dir;
bool disable_poison_input = false;
bool disable_undef_input = false;
bool tgt_is_asm = false;
bool fail_if_src_is_ub = false;
bool disallow_ub_exploitation = false;
bool debug = false;
unsigned src_unroll_cnt = 0;
unsigned tgt_unroll_cnt = 0;
unsigned max_offset_bits = 64;
unsigned max_sizet_bits = 64;
FpMappingMode fp_mapping_mode = FpMappingMode::FloatingPoint;

ostream &dbg() {
  return *debug_os;
}

void set_debug(ostream &os) {
  debug_os = &os;
}

bool is_uf_float() {
  return fp_mapping_mode == FpMappingMode::UninterpretedFunctions;
}

}
