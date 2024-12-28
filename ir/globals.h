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

extern unsigned num_inaccessiblememonly_fns;

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

/// >= bits_size_t && <= bits_program_pointer
extern unsigned bits_ptr_address;

/// Number of bits for a byte.
extern unsigned bits_byte;

/// Required bits to store the size of sub-byte accesses
/// (e.g., store i5 -> we record 4, so 3 bits)
extern unsigned num_sub_byte_bits;

extern unsigned strlen_unroll_cnt;
extern unsigned memcmp_unroll_cnt;

extern bool little_endian;

/// Whether pointer addresses are observed
extern bool observes_addresses;
extern bool has_int2ptr;

/// Whether there is an alloca
extern bool has_alloca;

extern bool has_fncall;

// has a function call that writes to global memory (not-inaccessible only)
extern bool has_write_fncall;

/// Whether any function argument (not function call arg) has the attribute
extern bool has_nocapture;
extern bool has_noread;
extern bool has_nowrite;
extern bool has_ptr_arg;
extern bool has_initializes_attr;

/// Whether there null pointers appear in the program
extern bool has_null_pointer;

/// Whether the null block should be allocated
extern bool has_null_block;

extern bool null_is_dereferenceable;

/// Whether there is at least one global with different alignment in src/tgt
extern bool has_globals_diff_align;

/// Whether the programs do memory accesses that load/store int/ptrs
extern bool does_int_mem_access;
extern bool does_ptr_mem_access;
extern bool does_ptr_store;

extern unsigned heap_block_alignment;

extern bool has_indirect_fncalls;

}
