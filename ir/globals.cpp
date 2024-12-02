// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/globals.h"
#include "smt/expr.h"
#include <string_view>

using namespace smt;
using namespace std;

namespace IR {

unsigned num_locals_src = 128;
unsigned num_locals_tgt = 128;
unsigned num_consts_src = 128;
unsigned num_globals_src = 256;
unsigned num_ptrinputs = 64;
unsigned num_inaccessiblememonly_fns = 32;
unsigned num_nonlocals = 256;
unsigned num_nonlocals_src = 256;
unsigned bits_poison_per_byte = 8;
unsigned bits_for_ptrattrs = 8;
unsigned bits_for_bid = 64;
unsigned bits_for_offset = 64;
unsigned bits_program_pointer = 64;
unsigned bits_size_t = 64;
unsigned bits_ptr_address = 64;
unsigned bits_byte = 8;
unsigned num_sub_byte_bits = 6;
unsigned strlen_unroll_cnt = 8;
unsigned memcmp_unroll_cnt = 8;
bool little_endian = true;
bool observes_addresses = true;
bool has_int2ptr = true;
bool has_alloca = true;
bool has_fncall = true;
bool has_write_fncall = true;
bool has_nocapture = true;
bool has_noread = true;
bool has_nowrite = true;
bool has_ptr_arg = true;
bool has_initializes_attr = true;
bool has_null_block = true;
bool null_is_dereferenceable = false;
bool does_int_mem_access = true;
bool does_ptr_mem_access = true;
bool does_ptr_store = true;
unsigned heap_block_alignment = 8;
bool has_indirect_fncalls = true;


bool isUndef(const expr &e) {
  expr var;
  unsigned h, l;
  if (e.isExtract(var, h, l))
    return isUndef(var);

  return string_view(e.fn_name()).substr(0, 6) == "undef!";
}

}
