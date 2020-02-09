#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

namespace smt { class expr; }

namespace IR {

/// Upperbound of the number of non-local pointers returned by instructions
extern unsigned num_max_nonlocals_inst;

/// Upperbound of the number of local blocks (max(src, tgt))
extern unsigned num_locals;

// Upperbound of the number of nonlocal blocks
extern unsigned num_nonlocals;

/// Number of bits needed for attributes of pointers (e.g. nocapture).
extern unsigned bits_for_ptrattrs;

/// Number of bits needed for encoding a memory block id
extern unsigned bits_for_bid;

// Number of bits needed for encoding a pointer's offset
extern unsigned bits_for_offset;

/// Size of a program pointer in bytes
extern unsigned bits_program_pointer;

/// sizeof(size_t)
extern unsigned bits_size_t;

/// Number of bits for a byte.
extern unsigned bits_byte;

extern bool little_endian;

/// Whether int2ptr or ptr2int are used in either function
extern bool has_int2ptr;
extern bool has_ptr2int;

/// Whether malloc or free/delete is used in either function
extern bool has_malloc;
extern bool has_free;

extern bool has_fncall;

/// Whether any function argument (not function call arg) has the attribute
extern bool has_nocapture;
extern bool has_readonly;
extern bool has_readnone;

/// Whether there are allocas that are initially dead (need start_lifetime)
extern bool has_dead_allocas;

/// Whether the programs do memory accesses that load/store int/ptrs
extern bool does_int_mem_access;
extern bool does_ptr_mem_access;
extern bool does_ptr_store;

/// Whether the programs do memory accesses of less than bits_byte
extern bool does_sub_byte_access;


bool isUndef(const smt::expr &e);

}
