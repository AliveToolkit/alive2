// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/globals.h"

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
bool nullptr_is_used;
bool has_int2ptr;
bool has_ptr2int;
bool has_malloc;
bool has_free;
bool has_nocapture;
bool has_readonly;
bool has_dead_allocas;
bool does_int_mem_access;
bool does_ptr_mem_access;
bool does_ptr_store;

}
