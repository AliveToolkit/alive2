// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/globals.h"
#include "smt/expr.h"
#include <string_view>

using namespace smt;
using namespace std;

namespace IR {

unsigned num_locals_src;
unsigned num_locals_tgt;
unsigned num_consts_src;
unsigned num_globals_src;
unsigned num_ptrinputs;
unsigned num_nonlocals;
unsigned num_nonlocals_src;
unsigned bits_poison_per_byte;
unsigned bits_for_ptrattrs;
unsigned bits_for_bid;
unsigned bits_for_offset;
unsigned bits_program_pointer;
unsigned bits_size_t;
unsigned bits_ptr_address;
unsigned bits_byte;
unsigned strlen_unroll_cnt;
unsigned memcmp_unroll_cnt;
bool little_endian;
bool observes_addresses;
bool has_malloc;
bool has_free;
bool has_alloca;
bool has_fncall;
bool has_nocapture;
bool has_noread;
bool has_nowrite;
bool has_dead_allocas;
bool has_null_block;
bool does_int_mem_access;
bool does_ptr_mem_access;
bool does_ptr_store;
unsigned heap_block_alignment;


bool isUndef(const expr &e) {
  auto name = e.fn_name();
  return string_view(name).substr(0, 6) == "undef!";
}

}
