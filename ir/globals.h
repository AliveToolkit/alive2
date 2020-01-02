#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

namespace IR {

/// Upperbound of the number of non-local pointers returned by instructions
extern unsigned num_max_nonlocals_inst;

/// Upperbound of the number of local blocks (max(src, tgt))
extern unsigned num_locals;

// Upperbound of the number of nonlocal blocks
extern unsigned num_nonlocals;

/// Number of bits needed for encoding a memory block id
extern unsigned bits_for_bid;

// Number of bits needed for encoding a pointer's offset
extern unsigned bits_for_offset;

/// sizeof(size_t). This is assume to be equal to pointer size
extern unsigned bits_size_t;

/// Number of bits for a byte.
extern unsigned bits_byte;

extern bool little_endian;

/// Whether the null pointer is used in either function
extern bool nullptr_is_used;

/// Whether int2ptr or ptr2int are used in either function
extern bool has_int2ptr;
extern bool has_ptr2int;

/// Whether malloc or free/delete is used in either function
extern bool has_malloc;
extern bool has_free;

/// Whether there are allocas that are initially dead (need start_lifetime)
extern bool has_dead_allocas;

/// Whether the programs do memory accesses that load/store int/ptrs
extern bool does_int_mem_access;
extern bool does_ptr_mem_access;

}
