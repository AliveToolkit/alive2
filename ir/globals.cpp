// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/globals.h"
#include "smt/expr.h"
#include <string_view>

using namespace smt;
using namespace std;

namespace IR {

unsigned num_max_nonlocals_inst;
unsigned num_locals;
unsigned num_nonlocals;
unsigned bits_for_ptrattrs;
unsigned bits_for_bid;
unsigned bits_for_offset;
unsigned bits_program_pointer;
unsigned bits_size_t;
unsigned bits_byte;
bool little_endian;
bool has_int2ptr;
bool has_ptr2int;
bool has_malloc;
bool has_free;
bool has_fncall;
bool has_nocapture;
bool has_readonly;
bool has_readnone;
bool has_dead_allocas;
bool does_int_mem_access;
bool does_ptr_mem_access;
bool does_ptr_store;
bool does_sub_byte_access;


bool isUndef(const expr &e) {
  auto name = e.fn_name();
  return string_view(name).substr(0, 6) == "undef!";
}

}
