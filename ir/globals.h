#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

namespace smt { class expr; }

namespace IR {

/// Upperbound of the number of local blocks
extern unsigned num_locals_src, num_locals_tgt;

/// Number of constant global variables in src
extern unsigned num_consts_src;

extern unsigned num_globals_src;

extern unsigned num_ptrinputs;

/// Number of non-constant globals introduced in tgt
extern unsigned num_extra_nonconst_tgt;

// Upperbound of the number of nonlocal blocks
extern unsigned num_nonlocals;

// Upperbound of the number of nonlocal blocks in src (<= num_nonlocals)
extern unsigned num_nonlocals_src;

extern unsigned bits_poison_per_byte;

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

extern unsigned strlen_unroll_cnt;
extern unsigned memcmp_unroll_cnt;

extern bool little_endian;

/// Whether int2ptr or ptr2int are used in either function
extern bool has_int2ptr;
extern bool has_ptr2int;

/// Whether malloc or free/delete is used in either function
extern bool has_malloc;
extern bool has_free;
/// Whether there is an alloca
extern bool has_alloca;

extern bool has_fncall;

/// Whether any function argument (not function call arg) has the attribute
extern bool has_nocapture;
extern bool has_readonly;
extern bool has_readnone;

/// Whether there are allocas that are initially dead (need start_lifetime)
extern bool has_dead_allocas;

/// Whether there is a pointer that can point to the null block
/// ex) undef ptr constant, fn arg
extern bool has_null_block;

/// Whether the programs do memory accesses that load/store int/ptrs
extern bool does_int_mem_access;
extern bool does_ptr_mem_access;
extern bool does_ptr_store;

extern unsigned heap_block_alignment;


bool isUndef(const smt::expr &e);
bool isTyVar(const smt::expr &ty, const smt::expr &var);

}
