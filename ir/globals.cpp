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
unsigned num_extra_nonconst_tgt;
unsigned num_nonlocals;
unsigned num_nonlocals_src;
unsigned bits_for_ptrattrs;
unsigned bits_for_bid;
unsigned bits_for_offset;
unsigned bits_program_pointer;
unsigned bits_size_t;
unsigned bits_byte;
unsigned strlen_unroll_cnt;
unsigned memcmp_unroll_cnt;
bool little_endian;
bool has_int2ptr;
bool has_ptr2int;
bool has_malloc;
bool has_free;
bool has_alloca;
bool has_fncall;
bool has_nocapture;
bool has_readonly;
bool has_readnone;
bool has_dead_allocas;
bool has_null_block;
bool does_int_mem_access;
bool does_ptr_mem_access;
bool does_ptr_store;
bool does_sub_byte_access;
unsigned heap_block_alignment;


bool isUndef(const expr &e) {
  auto name = e.fn_name();
  return string_view(name).substr(0, 6) == "undef!";
}

bool isTyVar(const expr &ty, const expr &var) {
  auto ty_name = ty.fn_name();
  auto var_name = var.fn_name();
  return string_view(ty_name).substr(0, 3) == "ty_" &&
         string_view(ty_name).substr(3) == var_name;
}

}
