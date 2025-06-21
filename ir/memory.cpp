// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/memory.h"
#include "ir/function.h"
#include "ir/globals.h"
#include "ir/state.h"
#include "ir/value.h"
#include "smt/solver.h"
#include "util/compiler.h"
#include "util/config.h"
#include <algorithm>
#include <array>
#include <numeric>
#include <string>

#define MAX_STORED_PTRS_SET 3

using namespace IR;
using namespace smt;
using namespace std;
using namespace util;

// Non-local block ids (assuming that no block is optimized out):
// 1. null block: has_null_block
// 2. global vars in source:
//      + num_consts_src    (constant globals)
//      + num_globals_src   (all globals incl constant)
// 3. pointer argument inputs:
//      has_null_block + num_globals_src + num_ptrinputs
// 4. nonlocal blocks returned by loads/calls:
//      has_null_block + num_globals_src + num_ptrinputs + 1 -> ...
// 5. a block reserved for encoding the memory touched by calls:
//      num_nonlocals_src - num_inaccessiblememonly_fns - has_write_fncall
// 6. 1 block per inaccessiblememonly
//      + num_inaccessiblememonly_fns ~ num_nonlocals_src - 1
// 7. constant global vars in target only:
//      num_nonlocals_src ~ num_nonlocals - 1
//            (constant globals in target only)

//--- Functions for non-local block analysis based on bid ---//

static bool skip_null() {
  return has_null_block && !null_is_dereferenceable;
}

// If include_tgt is true, return true if bid is a global var existing in target
// only as well
static bool is_globalvar(unsigned bid, bool include_tgt) {
  bool srcglb = has_null_block <= bid && bid < has_null_block + num_globals_src;
  bool tgtglb = num_nonlocals_src <= bid && bid < num_nonlocals;
  return srcglb || (include_tgt && tgtglb);
}

static bool is_constglb(unsigned bid, bool src_only = false) {
  if (has_null_block <= bid && bid < num_consts_src + has_null_block) {
    // src constglb
    assert(is_globalvar(bid, false));
    return true;
  }
  if (!src_only && num_nonlocals_src <= bid && bid < num_nonlocals) {
    // tgt constglb
    assert(!is_globalvar(bid, false) &&
            is_globalvar(bid, true));
    return true;
  }
  return false;
}

// Return true if bid is the nonlocal block used to encode function calls' side
// effects
static unsigned get_fncallmem_bid() {
  assert(has_write_fncall || num_inaccessiblememonly_fns > 0);
  return num_nonlocals_src - num_inaccessiblememonly_fns - has_write_fncall;
}

static bool is_fncall_mem(unsigned bid) {
  if (!has_write_fncall && num_inaccessiblememonly_fns == 0)
    return false;
  return bid >= get_fncallmem_bid() && bid < num_nonlocals_src;
}

static void ensure_non_fncallmem(const Pointer &p) {
  if (!p.isLocal().isFalse())
    return;
  uint64_t ubid;
  assert(!p.getShortBid().isUInt(ubid) || !is_fncall_mem(ubid));
  (void)ubid;
}

// bid: nonlocal block id
static bool always_alive(unsigned bid) {
  return // globals are always live
         is_globalvar(bid, true) ||
         // We can assume that bid is always live if it is a fncall mem.
         // Otherwise, fncall cannot write to the block.
         is_fncall_mem(bid);
}

// bid: nonlocal block id
static bool always_noread(unsigned bid, bool is_fncall_accessible = false) {
  return (!null_is_dereferenceable && bid < has_null_block) ||
         (!is_fncall_accessible && is_fncall_mem(bid));
}

// bid: nonlocal block id
static bool always_nowrite(unsigned bid, bool src_only = false,
                           bool is_fncall_accessible = false) {
  return always_noread(bid, is_fncall_accessible) || is_constglb(bid, src_only);
}

static expr mk_block_if(const expr &cond, expr then, expr els) {
  if (cond.isTrue())
    return then;
  if (cond.isFalse())
    return els;

  bool is_bv1 = then.isBV();
  bool is_bv2 = els.isBV();
  if (is_bv1 != is_bv2) {
    expr offset = expr::mkUInt(0, Pointer::bitsShortOffset());
    if (is_bv1)
      then = expr::mkConstArray(offset, then);
    else
      els  = expr::mkConstArray(offset, els);
  }
  return expr::mkIf(cond, then, els);
}


static unsigned next_local_bid;
static unsigned next_const_bid;
static unsigned next_global_bid;
static unsigned next_ptr_input;


static unsigned size_byte_number() {
  if (!num_sub_byte_bits)
    return 0;
  return
    max(ilog2_ceil(divide_up(1 << num_sub_byte_bits, bits_byte), false), 1u);
}

static unsigned sub_byte_bits() {
  return num_sub_byte_bits + size_byte_number();
}

static bool byte_has_ptr_bit() {
  return true;
}

static unsigned bits_ptr_byte_offset() {
  assert(!does_ptr_mem_access || bits_byte <= bits_program_pointer);
  return bits_byte < bits_program_pointer ? 3 : 0;
}

static unsigned padding_ptr_byte() {
  return Byte::bitsByte() - byte_has_ptr_bit() - 1 - Pointer::totalBits()
                          - bits_ptr_byte_offset();
}

static unsigned padding_nonptr_byte() {
  return
    Byte::bitsByte() - byte_has_ptr_bit() - bits_byte - bits_poison_per_byte
                     - num_sub_byte_bits - size_byte_number();
}

static expr concat_if(const expr &ifvalid, expr &&e) {
  return ifvalid.isValid() ? ifvalid.concat(e) : std::move(e);
}

static string local_name(const State *s, const char *name) {
  return string(name) + (s->isSource() ? "_src" : "_tgt");
}

static bool align_ge_size(const expr &align, const expr &size) {
  uint64_t algn, sz;
  return align.isUInt(algn) && size.isUInt(sz) && (1ull << algn) >= sz;
}

static bool align_gt_size(const expr &align, const expr &size) {
  uint64_t algn, sz;
  return align.isUInt(algn) && size.isUInt(sz) && (1ull << algn) > sz;
}

// Assumes that both begin + len don't overflow
static expr disjoint(const expr &begin1, const expr &len1, const expr &align1,
                     const expr &begin2, const expr &len2, const expr &align2) {
  // if blocks have the same alignment they can't start in the middle of
  // each other. We just need to ensure they have a different addr.
  if (align1.eq(align2) && align_ge_size(align1, len1) &&
      align_ge_size(align2, len2))
    return begin1 != begin2;
  return begin1.uge(begin2 + len2) || begin2.uge(begin1 + len1);
}

static expr load_bv(const expr &var, const expr &idx0) {
  auto bw = var.bits();
  if (!bw)
    return {};
  if (var.isAllOnes())
    return true;
  auto idx = idx0.zextOrTrunc(bw);
  return var.lshr(idx).extract(0, 0) == 1;
}

static void store_bv(Pointer &p, const expr &val, expr &local,
                     expr &non_local, bool assume_local = false,
                     const expr &cond = true) {
  auto bid0 = p.getShortBid();

  auto set = [&](const expr &var) {
    auto bw = var.bits();
    if (!bw)
      return expr();
    auto bid = bid0.zextOrTrunc(bw);
    auto one = expr::mkUInt(1, var) << bid;
    auto full = expr::mkInt(-1, bid);
    auto mask = (full << (bid + expr::mkUInt(1, var))) |
                full.lshr(expr::mkUInt(bw, var) - bid);
    return expr::mkIf(val, var | one, var & mask);
  };

  auto is_local = p.isLocal() || assume_local;
  local = mkIf_fold(cond && is_local, set(local), local);
  non_local = mkIf_fold(cond && !is_local, set(non_local), non_local);
}

namespace IR {

Byte::Byte(const Memory &m, expr &&byterepr) : m(m), p(std::move(byterepr)) {
  assert(!p.isValid() || p.bits() == bitsByte());
}

Byte::Byte(const Memory &m, const StateValue &ptr, unsigned i) : m(m) {
  // TODO: support pointers larger than 64 bits.
  assert(bits_program_pointer <= 64 && bits_program_pointer % 8 == 0);
  assert(i == 0 || bits_ptr_byte_offset() > 0);

  if (!does_ptr_mem_access) {
    p = expr::mkUInt(0, bitsByte());
    return;
  }

  if (byte_has_ptr_bit())
    p = expr::mkUInt(1, 1);
  p = concat_if(p,
                expr::mkIf(ptr.non_poison, expr::mkUInt(1, 1),
                           expr::mkUInt(0, 1)))
      .concat(ptr.value);
  if (bits_ptr_byte_offset())
    p = p.concat(expr::mkUInt(i, bits_ptr_byte_offset()));
  p = p.concat_zeros(padding_ptr_byte());
  assert(!ptr.isValid() || p.bits() == bitsByte());
}

Byte::Byte(const Memory &m, const StateValue &v, unsigned bits_read,
           unsigned byte_number)
  : m(m) {
  assert(!v.isValid() || v.value.bits() == bits_byte);
  assert(!v.isValid() || v.non_poison.isBool() ||
         v.non_poison.bits() == bits_poison_per_byte);

  if (!does_int_mem_access) {
    p = expr::mkUInt(0, bitsByte());
    return;
  }

  if (byte_has_ptr_bit())
    p = expr::mkUInt(0, 1);
  expr np = v.non_poison.isBool()
              ? expr::mkIf(v.non_poison,
                           expr::mkInt(-1, bits_poison_per_byte),
                           expr::mkUInt(0, bits_poison_per_byte))
              : v.non_poison;
  p = concat_if(p, np.concat(v.value));
  if (num_sub_byte_bits) {
    // optimization: byte number doesn't matter in assembly
    if (m.isAsmMode()) {
      p = p.concat_zeros(sub_byte_bits());
    } else {
      if ((bits_read % 8) == 0)
        bits_read = 0;
      p = p.concat(expr::mkUInt(bits_read, num_sub_byte_bits)
                     .concat(expr::mkUInt(byte_number, size_byte_number())));
    }
  }
  p = p.concat_zeros(padding_nonptr_byte());
  assert(!p.isValid() || p.bits() == bitsByte());
}

Byte Byte::mkPoisonByte(const Memory &m) {
  return { m, StateValue(expr::mkUInt(0, bits_byte), false), 0, true };
}

expr Byte::isPtr() const {
  return p.sign() == 1;
}

expr Byte::ptrNonpoison() const {
  auto bit = p.bits() - 1 - byte_has_ptr_bit();
  return isAsmMode() ? expr(true) : p.extract(bit, bit) == 1;
}

Pointer Byte::ptr() const {
  return { m, ptrValue() };
}

expr Byte::ptrValue() const {
  if (!does_ptr_mem_access)
    return expr::mkUInt(0, Pointer::totalBits());

  auto start = bits_ptr_byte_offset() + padding_ptr_byte();
  return p.extract(Pointer::totalBits() + start - 1, start);
}

expr Byte::ptrByteoffset() const {
  if (!does_ptr_mem_access)
    return expr::mkUInt(0, bits_ptr_byte_offset());
  if (bits_ptr_byte_offset() == 0)
    return expr::mkUInt(0, 1);

  unsigned start = padding_ptr_byte();
  return p.extract(bits_ptr_byte_offset() + start - 1, start);
}

expr Byte::nonptrNonpoison() const {
  if (!does_int_mem_access)
    return expr::mkUInt(0, 1);
  if (isAsmMode())
    return expr::mkInt(-1, bits_poison_per_byte);

  unsigned start = padding_nonptr_byte() + bits_byte + sub_byte_bits();
  return p.extract(start + bits_poison_per_byte - 1, start);
}

expr Byte::boolNonptrNonpoison() const {
  expr np = nonptrNonpoison();
  return np == expr::mkInt(-1, np);
}

expr Byte::nonptrValue() const {
  if (!does_int_mem_access)
    return expr::mkUInt(0, bits_byte);
  unsigned start = padding_nonptr_byte() + sub_byte_bits();
  return p.extract(start + bits_byte - 1, start);
}

expr Byte::numStoredBits() const {
  unsigned start = padding_nonptr_byte() + size_byte_number();
  return p.extract(start + num_sub_byte_bits - 1, start);
}

expr Byte::byteNumber() const {
  unsigned start = padding_nonptr_byte();
  return p.extract(start + size_byte_number() - 1, start);
}

expr Byte::isPoison() const {
  if (!does_int_mem_access)
    return does_ptr_mem_access ? !ptrNonpoison() : true;
  if (isAsmMode())
    return false;

  expr np = nonptrNonpoison();
  if (byte_has_ptr_bit() && bits_poison_per_byte == 1) {
    assert(!np.isValid() || ptrNonpoison().eq(np == 1));
    return np != 1;
  }
  return expr::mkIf(isPtr(), !ptrNonpoison(), np != expr::mkInt(-1, np));
}

expr Byte::nonPoison() const {
  if (isAsmMode())
    return expr::mkInt(-1, bits_poison_per_byte);
  if (!does_int_mem_access)
    return ptrNonpoison();

  expr np = nonptrNonpoison();
  if (byte_has_ptr_bit() && bits_poison_per_byte == 1) {
    assert(!np.isValid() || ptrNonpoison().eq(np == 1));
    return np;
  }
  return expr::mkIf(isPtr(),
                    expr::mkIf(ptrNonpoison(), expr::mkInt(-1, np),
                                               expr::mkUInt(0, np)),
                    np);
}

expr Byte::isZero() const {
  return expr::mkIf(isPtr(), ptr().isNull(), nonptrValue() == 0);
}

bool Byte::isAsmMode() const {
  return m.isAsmMode();
}

expr Byte::castPtrToInt() const {
  auto offset = ptrByteoffset().zextOrTrunc(bits_ptr_address);
       offset = offset * expr::mkUInt(bits_byte, offset);
  return ptr().getAddress().lshr(offset).zextOrTrunc(bits_byte);
}

expr Byte::forceCastToInt() const {
  return expr::mkIf(isPtr(), castPtrToInt(), nonptrValue());
}

expr Byte::refined(const Byte &other) const {
  bool asm_mode = other.m.isAsmMode();
  expr is_ptr = isPtr();
  expr is_ptr2 = other.isPtr();

  // allow int -> ptr type punning in asm mode
  // this is only need to support removal of 'store int undef'
  expr v1 = nonptrValue();
  expr v2 = asm_mode ? other.forceCastToInt() : other.nonptrValue();
  expr np1 = nonptrNonpoison();
  expr np2 = asm_mode ? other.nonPoison() : other.nonptrNonpoison();

  // int byte
  expr int_cnstr = true;
  if (does_int_mem_access) {
    if (bits_poison_per_byte == bits_byte) {
      int_cnstr = (np2 & np1) == np1 && (v1 & np1) == (v2 & np1);
    }
    else if (bits_poison_per_byte > 1) {
      assert((bits_byte % bits_poison_per_byte) == 0);
      unsigned bits_val = bits_byte / bits_poison_per_byte;
      for (unsigned i = 0; i < bits_poison_per_byte; ++i) {
        expr ev1 = v1.extract((i+1) * bits_val - 1, i * bits_val);
        expr ev2 = v2.extract((i+1) * bits_val - 1, i * bits_val);
        expr enp1 = np1.extract(i, i);
        expr enp2 = np2.extract(i, i);
        int_cnstr
          &= enp1 == 0 || ((enp1.eq(enp2) ? true : enp2 == 1) && ev1 == ev2);
      }
    } else {
      assert(!np1.isValid() || np1.bits() == 1);
      int_cnstr = np1 == 0 || ((np1.eq(np2) ? true : np2 == 1) && v1 == v2);
    }
  }

  expr ptr_cnstr;

  // fast path: if we didn't do any ptr store, then all ptrs in memory were
  // already there and don't need checking
  if (!does_ptr_store || is_ptr.isFalse() || (!asm_mode && is_ptr2.isFalse())) {
    ptr_cnstr = *this == other;
  } else {
    // allow ptr -> int type punning in asm mode
    expr extra = false;
    if (asm_mode) {
      extra = !is_ptr2 &&
              other.boolNonptrNonpoison() &&
              castPtrToInt() == other.nonptrValue();
    }
    ptr_cnstr = ptrNonpoison().implies(
                  (other.ptrNonpoison() &&
                   ptrByteoffset() == other.ptrByteoffset() &&
                   ptr().refined(other.ptr())) ||
                  extra);
  }

  expr constr = expr::mkIf(is_ptr, ptr_cnstr, int_cnstr);
  if (asm_mode)
    return constr;

  return expr::mkIf(is_ptr == is_ptr2,
                    constr,
                    // allow null ptr <-> zero
                    isPoison() ||
                      (isZero() && !other.isPoison() && other.isZero()));
}

unsigned Byte::bitsByte() {
  unsigned ptr_bits = does_ptr_mem_access *
                        (1 + Pointer::totalBits() + bits_ptr_byte_offset());
  unsigned int_bits = does_int_mem_access * (bits_byte + bits_poison_per_byte)
                        + sub_byte_bits();
  // allow at least 1 bit if there's no memory access
  return max(1u, byte_has_ptr_bit() + max(ptr_bits, int_bits));
}

ostream& operator<<(ostream &os, const Byte &byte) {
  if (byte.isPtr().isTrue()) {
    if (byte.ptrNonpoison().isTrue()) {
      os << byte.ptr() << ", byte offset=";
      byte.ptrByteoffset().printUnsigned(os);
    } else {
      os << "poison";
    }
  } else {
    auto np = byte.nonptrNonpoison();
    auto val = byte.nonptrValue();
    if (np.isZero())
      return os << "poison";

    if (np.isAllOnes()) {
      val.printHexadecimal(os);
    } else {
      os << "#b";
      for (unsigned i = 0; i < bits_poison_per_byte; ++i) {
        unsigned idx = bits_poison_per_byte - i - 1;
        auto is_poison = np.extract(idx, idx).isZero();
        auto v = val.extract(idx, idx).isAllOnes();
        os << (is_poison ? 'p' : (v ? '1' : '0'));
      }
    }

    uint64_t num_bits;
    if (num_sub_byte_bits &&
        byte.numStoredBits().isUInt(num_bits) && num_bits != 0) {
      os << " / written with " << num_bits << " bits";
      if (byte.byteNumber().isUInt(num_bits))
        os << " / byte #" << num_bits;
    }
  }
  return os;
}

unsigned Memory::bitsAlignmentInfo() {
  return ilog2_ceil(bits_size_t, false);
}

bool Memory::observesAddresses() {
  return true; //observes_addresses;
}

static bool isFnReturnValue(const expr &e) {
  expr arr, idx;
  if (e.isLoad(arr, idx))
    return isFnReturnValue(arr);

  expr val;
  unsigned hi, lo;
  if (e.isExtract(val, hi, lo))
    return isFnReturnValue(val);

  return e.fn_name().starts_with("#fnret_");
}

int Memory::isInitialMemBlock(const expr &e, bool match_any_init) {
  string_view name;
  expr load, blk, idx;
  unsigned hi, lo;
  if (e.isExtract(load, hi, lo) && load.isLoad(blk, idx))
    name = blk.fn_name();
  else
    name = e.fn_name();

  if (name.starts_with("init_mem_"))
    return 1;

  return match_any_init && name.starts_with("blk_val!") ? 2 : 0;
}

bool Memory::isInitialMemoryOrLoad(const expr &e, bool match_any_init) {
  expr arr, idx;
  if (e.isLoad(arr, idx))
    return isInitialMemoryOrLoad(arr, match_any_init);

  expr val;
  unsigned hi, lo;
  if (e.isExtract(val, hi, lo))
    return isInitialMemoryOrLoad(val, match_any_init);

  return isInitialMemBlock(e, match_any_init) != 0;
}
}

static void pad(StateValue &v, unsigned amount, State &s) {
  if (amount == 0)
    return;

  expr ty = expr::mkUInt(0, amount);
  auto pad = [&](expr &v) {
    expr var = expr::mkFreshVar("padding", ty);
    v = var.concat(v);
    s.addQuantVar(var);
  };

  pad(v.value);
  if (!v.non_poison.isBool())
    pad(v.non_poison);
}

static vector<Byte> valueToBytes(const StateValue &val, const Type &fromType,
                                 const Memory &mem, State &s) {
  vector<Byte> bytes;
  if (fromType.isPtrType()) {
    Pointer p(mem, val.value);
    unsigned bytesize = bits_program_pointer / bits_byte;

    // constant global can't store pointers that alias with local blocks
    if (s.isInitializationPhase() && !p.isLocal().isFalse()) {
      expr bid  = expr::mkUInt(0, 1).concat(p.getShortBid());
      p = Pointer(mem, bid, p.getOffset(), p.getAttrs());
    }

    for (unsigned i = 0; i < bytesize; ++i)
      bytes.emplace_back(mem, StateValue(expr(p()), expr(val.non_poison)), i);
  } else {
    assert(!fromType.isAggregateType() || isNonPtrVector(fromType));
    StateValue bvval = fromType.toInt(s, val);
    unsigned bitsize = bvval.bits();
    unsigned bytesize = divide_up(bitsize, bits_byte);

    // There are no sub-byte accesses in assembly
    if (mem.isAsmMode() && (bitsize % 8) != 0) {
      s.addUB(expr(false));
    }

    pad(bvval, bytesize * bits_byte - bitsize, s);
    unsigned np_mul = bits_poison_per_byte;

    for (unsigned i = 0; i < bytesize; ++i) {
      StateValue data {
        bvval.value.extract((i + 1) * bits_byte - 1, i * bits_byte),
        bvval.non_poison.extract((i + 1) * np_mul - 1, i * np_mul)
      };
      bytes.emplace_back(mem, data, bitsize, i);
    }
  }
  return bytes;
}

static StateValue bytesToValue(const Memory &m, const vector<Byte> &bytes,
                               const Type &toType) {
  assert(!bytes.empty());

  auto ub_pre = [&](expr &&e) -> expr {
    if (config::disallow_ub_exploitation) {
      m.getState().addPre(std::move(e));
      return true;
    }
    return std::move(e);
  };

  bool is_asm = m.isAsmMode();

  if (toType.isPtrType()) {
    assert(bytes.size() == bits_program_pointer / bits_byte);
    expr loaded_ptr, is_ptr;
    // The result is not poison if all of these hold:
    // (1) There's no poison byte, and they are all pointer bytes
    // (2) All of the bytes have the same information
    // (3) Byte offsets should be correct
    // A zero integer byte is considered as a null pointer byte with any byte
    // offset.
    expr non_poison = true;

    for (unsigned i = 0, e = bytes.size(); i < e; ++i) {
      auto &b = bytes[i];
      expr ptr_value = b.ptrValue();
      expr b_is_ptr  = b.isPtr();

      if (i == 0) {
        loaded_ptr = ptr_value;
        is_ptr     = std::move(b_is_ptr);
      } else {
        non_poison &= ub_pre(is_ptr == b_is_ptr);
      }

      non_poison &=
        ub_pre(expr::mkIf(is_ptr,
                          b.ptrByteoffset() == i && ptr_value == loaded_ptr,
                          b.nonptrValue() == 0 && does_int_mem_access));
      non_poison &= !b.isPoison();
    }
    if (is_asm)
      non_poison = true;

    // if bits of loaded ptr are a subset of the non-ptr value,
    // we know they must be zero otherwise the value is poison.
    // Therefore we obtain a null pointer for free.
    expr _;
    unsigned low, high, low2, high2;
    if (loaded_ptr.isExtract(_, high, low) &&
        bytes[0].nonptrValue().isExtract(_, high2, low2) &&
        high2 >= high && low2 <= low) {
      // do nothing
    } else {
      loaded_ptr = expr::mkIf(is_ptr, loaded_ptr, Pointer::mkNullPointer(m)());
    }
    return { std::move(loaded_ptr), std::move(non_poison) };

  } else {
    assert(!toType.isAggregateType() || isNonPtrVector(toType));
    auto bitsize = toType.bits();
    assert(divide_up(bitsize, bits_byte) == bytes.size());

    StateValue val;
    bool first = true;
    IntType ibyteTy("", bits_byte);
    unsigned byte_number = 0;

    for (auto &b: bytes) {
      expr expr_np = ub_pre(!b.isPtr());

      if (num_sub_byte_bits) {
        unsigned bits = (bitsize % 8) == 0 ? 0 : bitsize;
        expr_np &= b.numStoredBits() == bits;
        expr_np &= b.byteNumber() == byte_number++;
      }
      if (is_asm)
        expr_np = true;

      StateValue v(is_asm ? b.forceCastToInt() : b.nonptrValue(),
                   ibyteTy.combine_poison(expr_np, b.nonptrNonpoison()));
      val = first ? std::move(v) : v.concat(val);
      first = false;
    }
    return toType.fromInt(val.trunc(bitsize, toType.np_bits(true)));
  }
}

namespace IR {

Memory::AliasSet::AliasSet(const Memory &m)
  : local(m.numLocals(), false), non_local(m.numNonlocals(), false) {}

Memory::AliasSet::AliasSet(const Memory &m1, const Memory &m2)
  : local(max(m1.numLocals(), m2.numLocals()), false),
    non_local(max(m1.numNonlocals(), m2.numNonlocals()), false) {}

size_t Memory::AliasSet::size(bool islocal) const {
  return (islocal ? local : non_local).size();
}

int Memory::AliasSet::isFullUpToAlias(bool islocal) const {
  auto &v = islocal ? local : non_local;
  unsigned i = 0;
  for (unsigned e = v.size(); i != e; ++i) {
    if (!v[i])
      break;
  }
  for (unsigned i2 = i, e = v.size(); i2 != e; ++i2) {
    if (v[i])
      return -1;
  }
  return i - 1;
}

expr Memory::AliasSet::mayAlias(bool islocal, const expr &bid) const {
  int upto = isFullUpToAlias(islocal);
  if (upto >= 0)
    return bid.ule(upto);

  expr ret(false);
  for (unsigned i = 0, e = size(islocal); i < e; ++i) {
    if (mayAlias(islocal, i))
      ret |= bid == i;
  }
  return ret;
}

bool Memory::AliasSet::mayAlias(bool islocal, unsigned bid) const {
  return (islocal ? local : non_local)[bid];
}

unsigned Memory::AliasSet::numMayAlias(bool islocal) const {
  auto &v = islocal ? local : non_local;
  return count(v.begin(), v.end(), true);
}

void Memory::AliasSet::setMayAlias(bool islocal, unsigned bid) {
  (islocal ? local : non_local)[bid] = true;
}

void Memory::AliasSet::setMayAliasUpTo(bool local, unsigned limit) {
  for (unsigned i = 0; i <= limit; ++i) {
    setMayAlias(local, i);
  }
}

void Memory::AliasSet::setNoAlias(bool islocal, unsigned bid) {
  (islocal ? local : non_local)[bid] = false;
}

void Memory::AliasSet::intersectWith(const AliasSet &other) {
  auto intersect = [](auto &a, const auto &b) {
    auto I2 = b.begin(), E2 = b.end();
    for (auto I = a.begin(), E = a.end(); I != E && I2 != E2; ++I, ++I2) {
      *I = *I && *I2;
    }
  };
  intersect(local, other.local);
  intersect(non_local, other.non_local);
}

void Memory::AliasSet::unionWith(const AliasSet &other) {
  auto unionfn = [](auto &a, const auto &b) {
    auto I2 = b.begin(), E2 = b.end();
    for (auto I = a.begin(), E = a.end(); I != E && I2 != E2; ++I, ++I2) {
      *I = *I || *I2;
    }
  };
  unionfn(local, other.local);
  unionfn(non_local, other.non_local);
}

static const array<uint64_t, 5> alias_buckets_vals = { 1, 2, 3, 5, 10 };
static array<uint64_t, 6> alias_buckets_hits = { 0 };
static uint64_t only_local = 0, only_nonlocal = 0;

void Memory::AliasSet::computeAccessStats() const {
  auto nlocal = numMayAlias(true);
  auto nnonlocal = numMayAlias(false);

  if (nlocal > 0 && nnonlocal == 0)
    ++only_local;
  else if (nlocal == 0 && nnonlocal > 0)
    ++only_nonlocal;

  auto alias = nlocal + nnonlocal;
  for (unsigned i = 0; i < alias_buckets_vals.size(); ++i) {
    if (alias <= alias_buckets_vals[i]) {
      ++alias_buckets_hits[i];
      return;
    }
  }
  ++alias_buckets_hits.back();
}

void Memory::AliasSet::printStats(ostream &os) {
  double total
    = accumulate(alias_buckets_hits.begin(), alias_buckets_hits.end(), 0);

  if (!total)
    return;

  total /= 100.0;
  os.precision(1);
  os << fixed;

  os << "\n\nAlias sets statistics\n=====================\n"
        "Only local:     " << only_local
     << " (" << (only_local / total)
     << "%)\nOnly non-local: " << only_nonlocal
     << " (" << (only_nonlocal / total)
     << "%)\n\nBuckets:\n";

  for (unsigned i = 0; i < alias_buckets_vals.size(); ++i) {
    os << "\u2264 " << alias_buckets_vals[i] << ": "
       << alias_buckets_hits[i]
       << " (" << (alias_buckets_hits[i] / total) << "%)\n";
  }
  os << "> " << alias_buckets_vals.back() << ": "
     << alias_buckets_hits.back()
     << " (" << (alias_buckets_hits.back() / total) << "%)\n";
}

void Memory::AliasSet::print(ostream &os) const {
  auto print = [&](const char *str, const auto &v) {
    os << str;
    for (auto bit : v) {
      os << bit;
    }
  };

  bool has_local = false;
  if (numMayAlias(true) > 0) {
    print("local: ", local);
    has_local = true;
  }
  if (numMayAlias(false) > 0) {
    if (has_local) os << " / ";
    print("non-local: ", non_local);
  } else if (!has_local)
    os << "(empty)";
}

weak_ordering Memory::MemBlock::operator<=>(const MemBlock &rhs) const {
  // FIXME:
  // 1) xcode doesn't have tuple::operator<=>
  // 2) gcc has a bug and can't generate the default
  if (auto cmp = val   <=> rhs.val;   is_neq(cmp)) return cmp;
  if (auto cmp = undef <=> rhs.undef; is_neq(cmp)) return cmp;
  if (auto cmp = type  <=> rhs.type;  is_neq(cmp)) return cmp;
  return weak_ordering::equivalent;
}


static set<Pointer> all_leaf_ptrs(const Memory &m, const expr &ptr) {
  set<Pointer> ptrs;
  for (auto &ptr_val : ptr.leafs()) {
    ptrs.emplace(m, ptr_val);
  }
  return ptrs;
}

static set<expr> extract_possible_local_bids(Memory &m, const expr &eptr) {
  set<expr> ret;
  for (auto &ptr : all_leaf_ptrs(m, eptr)) {
    if (!ptr.isLocal().isFalse() && !ptr.isLogical().isFalse())
      ret.emplace(ptr.getShortBid());
  }
  return ret;
}

static unsigned max_program_nonlocal_bid() {
  return num_nonlocals_src-1 - num_inaccessiblememonly_fns - has_write_fncall;
}

unsigned Memory::nextNonlocalBid() {
  unsigned next = min(next_nonlocal_bid++, max_program_nonlocal_bid());
  assert(!is_fncall_mem(next));
  return next;
}

unsigned Memory::numCurrentNonLocals() const {
  unsigned bids = min(next_nonlocal_bid, max_program_nonlocal_bid());
  assert(!is_fncall_mem(bids));
  return bids + 1;
}

unsigned Memory::numLocals() const {
  return state->isSource() ? num_locals_src : num_locals_tgt;
}

unsigned Memory::numNonlocals() const {
  return state->isSource() ? num_nonlocals_src : num_nonlocals;
}

expr Memory::isBlockAlive(const expr &bid, bool local) const {
  uint64_t bid_n;
  if (!local && bid.isUInt(bid_n) && always_alive(bid_n))
    return true;

  return
    load_bv(local ? local_block_liveness : non_local_block_liveness, bid) &&
      (!local && has_null_block && !null_is_dereferenceable ? bid != 0 : true);
}

bool Memory::mayalias(bool local, unsigned bid0, const expr &offset0,
                      const expr &bytes, uint64_t align, bool write) const {
  if (local && bid0 >= next_local_bid)
    return false;

  if (!local) {
    if (bid0 >= numNonlocals())         return false;
    if (!write && always_noread(bid0))  return false;
    if ( write && always_nowrite(bid0)) return false;
  }

  if (state->isUndef(offset0))
    return false;

  expr bid = expr::mkUInt(bid0, Pointer::bitsShortBid());

  if (auto sz = (local ? local_blk_size : non_local_blk_size).lookup(bid)) {
    if (sz->ult(bytes).isTrue())
      return false;

    //expr offset = offset0.sextOrTrunc(bits_size_t);

#if 0
    // Never hits in practice
    // FIXME: this is for logical pointers
    if (auto blk_align0
          = (local ? local_blk_align : non_local_blk_align).lookup(bid)) {
      int64_t off;
      uint64_t blk_align;
      if (offset0.isInt(off) && off > 0 &&
          blk_align0->isUInt(blk_align) &&
          ((uint64_t)off % align) % (1ull << blk_align) != 0)
        return false;
    }
#endif

    if (local && !observed_addrs.mayAlias(true, bid0)) {
      // block align must be >= access align if the address hasn't been observed
      if (auto blk_align0 = local_blk_align.lookup(bid)) {
        uint64_t blk_align;
        if (blk_align0->isUInt(blk_align) &&
            (1ull << blk_align) < align)
          return false;
      }
    }

#if 0
    // Never hits in practice
    // FIXME: this is for logical pointers
    if (!isAsmMode()) {
      if (offset.uge(*sz).isTrue() ||
          (*sz - offset).ult(bytes).isTrue())
        return false;
    }
#endif
  } else if (local) // allocated in another branch
    return false;

  if (isBlockAlive(bid, local).isFalse())
    return false;

  return true;
}

static bool isDerivedFromLoad(const expr &e) {
  expr val, val2, _;
  unsigned h, l;
  if (e.isExtract(val, h, l))
    return isDerivedFromLoad(val);
  if (e.isIf(_, val, val2))
    return isDerivedFromLoad(val) && isDerivedFromLoad(val2);
  return e.isLoad(_, _);
}

Memory::AliasSet Memory::computeAliasing(const Pointer &ptr, const expr &bytes,
                                         uint64_t align, bool write) const {
  AliasSet aliasing(*this);
  auto sz_local    = min(next_local_bid, (unsigned)aliasing.size(true));
  auto sz_nonlocal = aliasing.size(false);

  auto check_alias = [&](AliasSet &alias, bool local, unsigned bid,
                         const expr &offset) {
    if (!alias.mayAlias(local, bid) &&
        mayalias(local, bid, offset, bytes, align, write))
      alias.setMayAlias(local, bid);
  };

  // collect over-approximation of possible touched bids
  for (auto &p : all_leaf_ptrs(*this, ptr())) {
    if (has_noread && !write && p.isNoRead().isTrue())
      continue;
    if (has_nowrite && write && p.isNoWrite().isTrue())
      continue;

    AliasSet this_alias = aliasing;
    auto is_local = p.isLocal();
    auto shortbid = p.getShortBid();
    expr offset   = p.getOffset();
    uint64_t bid;
    if (shortbid.isUInt(bid) &&
        (!isAsmMode() || state->isImplied(p.isInbounds(true), true))) {
      if (!is_local.isFalse() && bid < sz_local)
        check_alias(this_alias, true, bid, offset);
      if (!is_local.isTrue() && bid < sz_nonlocal)
        check_alias(this_alias, false, bid, offset);
      goto end;
    }

    {
    bool is_init_memory     = isInitialMemoryOrLoad(shortbid, false);
    bool is_from_fn_or_load = !is_init_memory &&
                              (isInitialMemoryOrLoad(shortbid, true) ||
                               isFnReturnValue(shortbid) ||
                               isDerivedFromLoad(shortbid));

    for (auto local : { true, false }) {
      if ((local && is_local.isFalse()) || (!local && is_local.isTrue()))
        continue;
      for (unsigned i = 0, e = local ? sz_local : sz_nonlocal; i < e; ++i) {
        // initial memory doesn't have local ptrs stored
        if (local && is_init_memory)
          continue;
        // functions and memory loads can only return escaped ptrs
        if (local &&
            is_from_fn_or_load &&
            !escaped_local_blks.mayAlias(true, i))
          continue;
        check_alias(this_alias, local, i, offset);
      }
    }
    }

end:
    // intersect computed aliasing with known aliasing
    auto I = ptr_alias.find(p.getBid());
    if (I != ptr_alias.end())
      this_alias.intersectWith(I->second);
    aliasing.unionWith(this_alias);
  }

  // intersect computed aliasing with known aliasing
  // We can't store the result since our cache is per Bid expression only
  // and doesn't take byte size, etc into account.
  auto I = ptr_alias.find(ptr.getBid());
  if (I != ptr_alias.end())
    aliasing.intersectWith(I->second);

  aliasing.computeAccessStats();
  return aliasing;
}

void Memory::access(const Pointer &ptr, const expr &bytes, uint64_t align,
                    bool write, const
                      function<void(MemBlock&, const Pointer&, unsigned, bool,
                                    expr&&)> &fn) {
  assert(!ptr.isLogical().isFalse());
  auto aliasing = computeAliasing(ptr, bytes, align, write);
  unsigned has_local = aliasing.numMayAlias(true);
  unsigned has_nonlocal = aliasing.numMayAlias(false);
  bool has_both = has_local && has_nonlocal;
  bool is_singleton = has_local + has_nonlocal == 1;

  expr addr = ptr.getAddress();
  expr offset = ptr.getOffset();
  expr is_local = ptr.isLocal();
  expr bid = has_both ? ptr.getBid() : ptr.getShortBid();
  expr one = expr::mkUInt(1, 1);

  auto sz_local = aliasing.size(true);
  auto sz_nonlocal = aliasing.size(false);

#define call_fn(block, local, cond_log)                                        \
    fn(block, Pointer(*this, i, local) + offset, i, local,                     \
       is_singleton ? expr(true) : (cond_log));

  for (unsigned i = 0; i < sz_local; ++i) {
    if (!aliasing.mayAlias(true, i))
      continue;

    auto n = expr::mkUInt(i, Pointer::bitsShortBid());
    call_fn(local_block_val[i], true,
            has_local == 1 ? is_local : (bid == (has_both ? one.concat(n) : n)))
  }

  for (unsigned i = 0; i < sz_nonlocal; ++i) {
    if (!aliasing.mayAlias(false, i))
      continue;

    // A nonlocal block for encoding fn calls' side effects cannot be
    // accessed.
    // If aliasing info says it can, either imprecise analysis or incorrect
    // block id encoding is happening.
    assert(!is_fncall_mem(i));

    call_fn(non_local_block_val[i], false,
            has_nonlocal == 1 ? !is_local : bid == i)
  }
}

static expr raw_load(const expr &block, const expr &offset,
                     uint64_t max_idx = UINT64_MAX) {
  return block.isBV() ? block : block.load(offset, max_idx);
}

static expr mk_store(expr mem, const expr &offset, const expr &val) {
  if (mem.isBV())
    mem = expr::mkConstArray(offset, mem);
  return mem.store(offset, val);
}

Byte Memory::raw_load(bool local, unsigned bid, const expr &offset) const {
  auto &block = (local ? local_block_val : non_local_block_val)[bid].val;
  return {*this, ::raw_load(block, offset) };
}

vector<Byte> Memory::load(const Pointer &ptr, unsigned bytes, set<expr> &undef,
                          uint64_t align, bool left2right, DataType type) {
  if (bytes == 0)
    return {};

  unsigned bytesz = (bits_byte / 8);
  unsigned loaded_bytes = bytes / bytesz;
  vector<DisjointExpr<expr>> loaded;
  expr poison = Byte::mkPoisonByte(*this)();
  loaded.resize(loaded_bytes, poison);

  auto fn = [&](MemBlock &blk, const Pointer &ptr, unsigned bid, bool local,
                expr &&cond) {
    bool is_poison = (type & blk.type) == DATA_NONE;
    if (is_poison) {
      for (unsigned i = 0; i < loaded_bytes; ++i) {
        loaded[i].add(poison, cond);
      }
    } else {
      uint64_t blk_size = UINT64_MAX;
      bool single_load
        = ptr.blockSizeAligned().isUInt(blk_size) && blk_size == bytes;
      auto offset      = ptr.getShortOffset();
      expr blk_offset  = single_load ? expr::mkUInt(0, offset) : offset;

      for (unsigned i = 0; i < loaded_bytes; ++i) {
        unsigned idx = left2right ? i : (loaded_bytes - i - 1);
        assert(idx < blk_size);
        uint64_t max_idx = blk_size - bytes + idx;
        expr off = blk_offset + expr::mkUInt(idx, offset);
        loaded[i].add(::raw_load(blk.val, off, max_idx), cond);
      }
      undef.insert(blk.undef.begin(), blk.undef.end());
    }
  };

  access(ptr, expr::mkUInt(bytes, bits_size_t), align, false, fn);

  vector<Byte> ret;
  for (auto &disj : loaded) {
    ret.emplace_back(*this, *std::move(disj)());
  }
  return ret;
}

Memory::DataType Memory::data_type(const vector<pair<unsigned, expr>> &data,
                                   bool full_store) const {
  if (isAsmMode())
    return DATA_ANY;

  unsigned ty = DATA_NONE;
  unsigned num_int_zeros = 0;
  for (auto &[idx, val] : data) {
    Byte byte(*this, expr(val));
    auto is_ptr = byte.isPtr();
    if (!is_ptr.isFalse())
      ty |= DATA_PTR;
    if (!is_ptr.isTrue()) {
      ty |= DATA_INT;
      num_int_zeros += !byte.isZero().isFalse();
    }
    if (ty == DATA_ANY)
      return DATA_ANY;
  }

  // allow 0 -> nullptr conversion
  if ((full_store && num_int_zeros >= bits_program_pointer / bits_byte) ||
      (!full_store && num_int_zeros > 0)) {
    ty |= DATA_PTR;
  }
  return DataType(ty);
}

void Memory::store(const Pointer &ptr,
                   const vector<pair<unsigned, expr>> &data,
                   const set<expr> &undef, uint64_t align) {
  if (data.empty())
    return;

  for (auto &[offset, val] : data) {
    Byte byte(*this, expr(val));
    escapeLocalPtr(byte.ptrValue(), byte.isPtr() && byte.ptrNonpoison());
  }

  unsigned bytes = data.size() * (bits_byte/8);

  auto stored_ty = data_type(data, false);
  auto stored_ty_full = data_type(data, true);

  auto fn = [&](MemBlock &blk, const Pointer &ptr, unsigned bid, bool local,
                expr &&cond) {
    auto mem = blk.val;

    uint64_t blk_size;
    bool full_write
      = ptr.blockSizeAligned().isUInt(blk_size) && blk_size == bytes;

    // optimization: if fully rewriting the block, don't bother with the old
    // contents. Pick a value as the default one.
    // If we are initializing const globals, the size may be larger than the
    // init because the size is rounded up to the alignment.
    // The remaining bytes are poison.
    if (full_write || state->isInitializationPhase()) {
      // optimization: if the access size == byte size, then we don't store
      // data as arrays, but simply as a BV
      if (data.size() == 1) {
        mem = data[0].second;
      } else {
        mem = full_write ? data[0].second : Byte::mkPoisonByte(*this)();
      }
      if (cond.isTrue()) {
        blk.undef.clear();
        blk.type = stored_ty_full;
      } else {
        blk.type |= stored_ty_full;
      }
    } else {
      blk.type |= stored_ty;
    }

    expr offset = ptr.getShortOffset();

    for (auto &[idx, val] : data) {
      expr off
        = offset + expr::mkUInt(idx >> Pointer::zeroBitsShortOffset(), offset);
      if (!local)
        record_stored_pointer(bid, off);

      if (full_write && val.eq(data[0].second))
        continue;
      mem = mk_store(std::move(mem), off, val);
    }
    blk.val = mk_block_if(cond, std::move(mem), std::move(blk.val));
    blk.undef.insert(undef.begin(), undef.end());
  };

  access(ptr, expr::mkUInt(bytes, bits_size_t), align,
         !state->isInitializationPhase(), fn);
}

void Memory::storeLambda(const Pointer &ptr, const expr &offset,
                         const expr &bytes,
                         const vector<pair<unsigned, expr>> &data,
                         const set<expr> &undef, uint64_t align) {
  assert(!state->isInitializationPhase());

  bool val_no_offset = data.size() == 1 && !data[0].second.vars().count(offset);
  auto stored_ty = data_type(data, false);

  expr val = data.back().second;
  expr mod = expr::mkUInt(data.size(), offset);
  for (auto I = next(data.rbegin()), E = data.rend(); I != E; ++I) {
    val = expr::mkIf(offset.urem(mod) == I->first, I->second, val);
  }

  auto fn = [&](MemBlock &blk, const Pointer &ptr, unsigned bid, bool local,
                expr &&cond) {
    auto orig_val = ::raw_load(blk.val, offset);

    // optimization: full rewrite
    if (bytes.eq(ptr.blockSizeAligned())) {
      blk.val = val_no_offset
        ? mk_block_if(cond, val, std::move(blk.val))
        : expr::mkLambda(offset, "#offset",
                         mk_block_if(cond, val, std::move(orig_val)));

      if (cond.isTrue()) {
        blk.undef.clear();
        blk.type = stored_ty;
      }
    } else {
      // offset in [ptr, ptr+sz)
      auto offset_cond = offset.uge(ptr.getShortOffset()) &&
                         offset.ult((ptr + bytes).getShortOffset());

      blk.val
        = expr::mkLambda(offset, "#offset",
                         mk_block_if(cond && offset_cond, val,
                                     std::move(orig_val)));
    }
    // const array -> BV
    blk.val.isConstArray(blk.val);

    if (!local) {
      auto &set = stored_pointers[bid];
      set.first.clear();
      set.second = true;
    }

    blk.type |= stored_ty;
    blk.undef.insert(undef.begin(), undef.end());
  };

  access(ptr, bytes.zextOrTrunc(bits_size_t), align, true, fn);
}


expr Memory::hasStored(const Pointer &p, const expr &bytes) const {
  assert(has_initializes_attr);

  unsigned bytes_per_byte = bits_byte / 8;
  expr bid = p.getShortBid();
  expr offset = p.getShortOffset();
  uint64_t bytes_i;
  if (bytes.isUInt(bytes_i) && (bytes_i / bytes_per_byte) <= 8) {
    expr ret = true;
    for (uint64_t off = 0; off < (bytes_i / bytes_per_byte); ++off) {
      expr off_expr = expr::mkUInt(off, offset);
      ret &= has_stored_arg.load(bid.concat(offset + off_expr));
    }
    return ret;
  } else {
    expr var = expr::mkFreshVar("#off", offset);
    state->addQuantVar(var);
    expr bytes_div = bytes.zextOrTrunc(offset.bits())
                          .udiv(expr::mkUInt(bytes_per_byte, bytes));
    return (var.uge(offset) && var.ult(offset + bytes_div))
             .implies(has_stored_arg.load(bid.concat(var)));
  }
}

void Memory::record_store(const Pointer &p, const smt::expr &bytes) {
  assert(has_initializes_attr);

  auto is_local = p.isLocal();
  if (is_local.isTrue())
    return;

  unsigned bytes_per_byte = bits_byte / 8;
  expr bid = p.getShortBid();
  expr offset = p.getShortOffset();
  uint64_t bytes_i;
  if (bytes.isUInt(bytes_i) && (bytes_i / bytes_per_byte) <= 8) {
    for (uint64_t off = 0; off < (bytes_i / bytes_per_byte); ++off) {
      expr off_expr = expr::mkUInt(off, offset);
      has_stored_arg
        = expr::mkIf(is_local,
                     has_stored_arg,
                     has_stored_arg.store(bid.concat(offset + off_expr), true));
    }
  } else {
    expr var     = expr::mkQVar(0, bid.concat(offset));
    expr var_bid = var.extract(var.bits()-1, offset.bits());
    expr var_off = var.trunc(offset.bits());

    expr bytes_div = bytes.zextOrTrunc(offset.bits())
                          .udiv(expr::mkUInt(bytes_per_byte, bytes));
    has_stored_arg
      = expr::mkIf(is_local,
                   has_stored_arg,
                   expr::mkLambda(var, "#bid_off",
                     (bid == var_bid &&
                      var_off.uge(offset) &&
                      var_off.ult(offset + bytes_div)) ||
                     has_stored_arg.load(var)));
  }
}

static bool memory_unused() {
  return num_locals_src == 0 && num_locals_tgt == 0 && num_nonlocals == 0;
}

expr Memory::mk_block_val_array(unsigned bid) const {
  auto str = "init_mem_" + to_string(bid);
  return Pointer(*this, bid, false).isBlkSingleByte()
    ? expr::mkVar(str.c_str(), Byte::bitsByte())
    : expr::mkArray(str.c_str(),
                    expr::mkUInt(0, Pointer::bitsShortOffset()),
                    expr::mkUInt(0, Byte::bitsByte()));
}

static expr mk_liveness_array() {
  if (!num_nonlocals)
    return {};

  // consider all non_locals are initially alive
  // block size can still be 0 to invalidate accesses
  return expr::mkInt(-1, num_nonlocals);
}

expr Memory::mkSubByteZExtStoreCond(const Byte &val, const Byte &val2) const {
  bool mk_axiom = &val == &val2;

  // optimization: the initial block is assumed to follow the ABI already.
  if (!mk_axiom && isInitialMemBlock(val.p, true))
    return true;

  bool always_1_byte = bits_byte >= (1u << num_sub_byte_bits);
  auto stored_bits   = val.numStoredBits();
  auto num_bytes
    = always_1_byte ? expr::mkUInt(0, size_byte_number())
                    : stored_bits.udiv(expr::mkUInt(bits_byte, stored_bits))
                        .zextOrTrunc(size_byte_number());
  auto leftover_bits
    = (always_1_byte ? stored_bits
                      : stored_bits.urem(expr::mkUInt(bits_byte, stored_bits))
      ).zextOrTrunc(bits_byte);

  return val.isPtr() ||
         (mk_axiom ? expr(false) : !val.boolNonptrNonpoison()) ||
         stored_bits == 0 ||
         val.byteNumber() != num_bytes ||
         val2.nonptrValue().lshr(leftover_bits) == 0;
}

void Memory::mkNonlocalValAxioms(const expr &block) const {
  expr offset = expr::mkQVar(0, Pointer::bitsShortOffset());
  const char *name = "#axoff";
  Byte byte(*this, ::raw_load(block, offset));

  // Users may request the initial memory to be non-poisonous
  if ((config::disable_poison_input && state->isSource()) &&
      (does_int_mem_access || does_ptr_mem_access)) {
    state->addAxiom(expr::mkForAll(1, &offset, &name, !byte.isPoison()));
  }

  if (!does_ptr_mem_access && !(num_sub_byte_bits && config::tgt_is_asm))
    return;

  // in ASM mode, non-byte-aligned values are expected to be zero-extended
  // per the ABI.
  if (num_sub_byte_bits && config::tgt_is_asm) {
    state->addAxiom(
      expr::mkForAll(1, &offset, &name, mkSubByteZExtStoreCond(byte, byte)));
  }

  if (!does_ptr_mem_access)
    return;

  Pointer loadedptr = byte.ptr();
  expr bid = loadedptr.getShortBid();

  unsigned upperbid = numNonlocals() - 1;
  expr bid_cond(true);
  if (has_fncall) {
    // initial memory cannot contain a pointer to fncall mem block.
    if (is_fncall_mem(upperbid)) {
      upperbid--;
    } else if (has_write_fncall && !num_inaccessiblememonly_fns) {
      assert(!state->isSource()); // target-only glb vars exist
      bid_cond = bid != get_fncallmem_bid();
    } else if (num_inaccessiblememonly_fns) {
      bid_cond = bid.ult(num_nonlocals_src - num_inaccessiblememonly_fns
                                            - has_write_fncall);
      if (upperbid > num_nonlocals_src)
        bid_cond |= bid.ugt(num_nonlocals_src - 1);
    }
  }
  bid_cond &= bid.ule(upperbid);

  state->addAxiom(
    expr::mkForAll(1, &offset, &name,
      byte.isPtr().implies(!loadedptr.isLocal(false) &&
                           !loadedptr.isNocapture(false) &&
                           bid_cond)));
}

bool Memory::isAsmMode() const {
  return state->getFn().has(FnAttrs::Asm);
}

Memory::Memory(State &state)
  : state(&state), escaped_local_blks(*this), observed_addrs(*this) {
  if (memory_unused())
    return;

  next_nonlocal_bid
    = has_null_block + num_globals_src + num_ptrinputs + has_fncall;

  if (skip_null())
    non_local_block_val.emplace_back();

  // TODO: should skip initialization of fully initialized constants
  for (unsigned bid = skip_null(), e = numNonlocals(); bid != e; ++bid) {
    non_local_block_val.emplace_back(mk_block_val_array(bid));
  }

  non_local_block_liveness = mk_liveness_array();

  // initialize all local blocks as non-pointer, poison value
  // This is okay because loading a pointer as non-pointer is also poison.
  if (numLocals() > 0) {
    auto poison_array
      = expr::mkConstArray(expr::mkUInt(0, Pointer::bitsShortOffset()),
                           Byte::mkPoisonByte(*this)());
    local_block_val.resize(numLocals(), {std::move(poison_array), DATA_NONE});

    // all local blocks are dead in the beginning
    local_block_liveness = expr::mkUInt(0, numLocals());
  }

  // no argument has been written to at entry
  if (has_initializes_attr) {
    unsigned bits = Pointer::bitsShortBid() + Pointer::bitsShortOffset();
    has_stored_arg = expr::mkConstArray(expr::mkUInt(0, bits), false);
  }

  stored_pointers.resize(numNonlocals());

  // Initialize a memory block for null pointer.
  if (skip_null()) {
    auto zero = expr::mkUInt(0, bits_size_t);
    alloc(&zero, bits_byte / 8, GLOBAL, false, false, 0);
  }
}

Memory Memory::dupNoRead() const {
  Memory ret(*state);
  // minimal state to allow fn calls to compare input fn ptrs that can't be read
  ret.local_blk_addr = local_blk_addr;
  ret.local_blk_align = local_blk_align;
  ret.local_blk_size = local_blk_size;
  ret.local_blk_kind = local_blk_kind;
  return ret;
}

void Memory::mkAxioms(const Memory &tgt) const {
  assert(state->isSource() && !tgt.state->isSource());
  if (memory_unused())
    return;

  // Non-local blocks cannot initially contain pointers to local blocks
  // and no-capture pointers.
  for (unsigned i = skip_null(), e = numNonlocals(); i != e; ++i) {
    mkNonlocalValAxioms(mk_block_val_array(i));
  }

  auto skip_bid = [&](unsigned bid) {
    if (bid == 0 && has_null_block)
      return true;
    if (is_globalvar(bid, true) || is_fncall_mem(bid))
      return false;
    return bid >= tgt.next_nonlocal_bid;
  };

  // transformation can increase alignment
  expr align = expr::mkUInt(ilog2(heap_block_alignment), bitsAlignmentInfo());

  if (null_is_dereferenceable && has_null_block) {
    state->addAxiom(Pointer::mkNullPointer(*this).blockAlignment() == 0);
    state->addAxiom(Pointer::mkNullPointer(tgt).blockAlignment() == 0);
  }

  for (unsigned bid = 0; bid < num_nonlocals_src; ++bid) {
    if (skip_bid(bid))
      continue;
    Pointer p(*this, bid, false);
    Pointer q(tgt, bid, false);
    auto p_align = p.blockAlignment();
    auto q_align = q.blockAlignment();
    state->addAxiom(
      p.isHeapAllocated().implies(p_align == align && q_align == align));
    if (!p_align.isConst() || !q_align.isConst())
      state->addAxiom(p_align == q_align);
  }
  for (unsigned bid = num_nonlocals_src; bid < num_nonlocals; ++bid) {
    if (skip_bid(bid))
      continue;
    Pointer q(tgt, bid, false);
    state->addAxiom(q.isHeapAllocated().implies(q.blockAlignment() == align));
  }

  if (!observesAddresses())
    return;

  if (has_null_block)
    state->addAxiom(Pointer::mkNullPointer(*this).getAddress(false) == 0);

  const unsigned max_quadratic_disjoint = 75;

  // Non-local blocks are disjoint.
  auto zero = expr::mkUInt(0, bits_ptr_address);
  auto one  = expr::mkUInt(1, zero);
  for (unsigned bid = 0; bid < num_nonlocals; ++bid) {
    if (skip_bid(bid))
      continue;

    // because tgt's align >= src's align, it's sufficient to have these
    // constraints for tgt.
    Pointer p1(tgt, bid, false);
    auto addr  = p1.getAddress();
    auto sz    = p1.blockSize().zextOrTrunc(bits_ptr_address);
    auto align = p1.blockAlignment();

    // limit max alignment so that aligning block size doesn't overflow
    state->addAxiom(align.ule_extend(bits_ptr_address-1));

    state->addAxiom(addr != zero);

    // address must be properly aligned
    auto align_bytes = one << align.zextOrTrunc(bits_ptr_address);
    state->addAxiom((addr & (align_bytes - one)) == zero);

    if (align_gt_size(align, sz)) {
      // if address is aligned and size < alignment, then it can't overflow
    } else if (align_ge_size(align, sz)) {
      // if size == alignment, addr+size can overflow only if addr == last
      auto last = zero - align_bytes;
      state->addAxiom(
        Pointer::hasLocalBit()
         ? addr.ult(
             expr::mkUInt(0, 1).concat(last.extract(bits_ptr_address-2, 0)))
         : addr != last);
    } else {
      sz = p1.blockSizeAligned().zextOrTrunc(bits_ptr_address);
      state->addAxiom(
        Pointer::hasLocalBit()
          // don't spill to local addr section
          ? (addr + sz).sign() == 0
          : addr.add_no_uoverflow(sz));
    }

    state->addAxiom(p1.blockSize()
                      .round_up_bits_no_overflow(p1.blockAlignment()));

    if (num_nonlocals > max_quadratic_disjoint)
      continue;

    // disjointness constraint
    for (unsigned bid2 = bid + 1; bid2 < num_nonlocals; ++bid2) {
      if (skip_bid(bid2))
        continue;
      Pointer p2(tgt, bid2, false);
      state->addAxiom(disjoint(addr, sz, align, p2.getAddress(),
                               p2.blockSizeAligned()
                                 .zextOrTrunc(bits_ptr_address),
                               p2.blockAlignment()));
    }
  }

  // tame down quadratic explosion in disjointness constraint with a quantifier.
  if (num_nonlocals > max_quadratic_disjoint) {
    auto bid_ty = expr::mkUInt(0, Pointer::bitsShortBid());
    expr bid1 = expr::mkQVar(0, bid_ty);
    expr bid2 = expr::mkQVar(1, bid_ty);
    expr offset = expr::mkUInt(0, bits_for_offset);
    Pointer p1(tgt, Pointer::mkLongBid(bid1, false), offset);
    Pointer p2(tgt, Pointer::mkLongBid(bid2, false), offset);
    expr vars[] = {bid1, bid2};
    const char *names[] = {"#bid1", "#bid2"};
    state->addAxiom(
      expr::mkForAll(2, vars, names,
        bid1 == bid2 ||
        disjoint(p1.getAddress(),
                 p1.blockSizeAligned().zextOrTrunc(bits_ptr_address),
                 p1.blockAlignment(),
                 p2.getAddress(),
                 p2.blockSizeAligned().zextOrTrunc(bits_ptr_address),
                 p2.blockAlignment())));
  }
}

void Memory::resetGlobals() {
  next_const_bid  = has_null_block;
  next_global_bid = has_null_block + num_consts_src;
  next_local_bid = 0;
  next_ptr_input = 0;
}

void Memory::syncWithSrc(const Memory &src) {
  assert(src.state->isSource() && !state->isSource());
  resetGlobals();
  next_const_bid  = num_nonlocals_src; // tgt consts start after all src vars
  // tgt can only have new const globals, but it allocates input ptrs
  next_global_bid = num_nonlocals_src;
  next_nonlocal_bid = src.next_nonlocal_bid;
  ptr_alias = src.ptr_alias;
  // TODO: copy alias info for fn return ptrs from src?
}

void Memory::markByVal(unsigned bid, bool is_const) {
  assert(is_globalvar(bid, false));
  byval_blks.emplace_back(bid, is_const);
}

expr Memory::mkInput(const char *name, const ParamAttrs &attrs0) {
  unsigned max_bid = has_null_block + num_globals_src + next_ptr_input++;
  assert(max_bid < num_nonlocals_src);

  // FIXME: doesn't consider physical ptrs
  // consider how to do POR?

  auto attrs = attrs0;
  attrs.set(ParamAttrs::IsArg);
  Pointer p(*this, name, attrs);
  auto bid = p.getShortBid();

  state->addAxiom(bid.ule(max_bid));
  state->addAxiom(!Pointer(*this, max_bid, false).isStackAllocated(false));

  AliasSet alias(*this);
  alias.setMayAliasUpTo(false, max_bid);

  for (auto [byval_bid, is_const] : byval_blks) {
    state->addAxiom(bid != byval_bid);
    alias.setNoAlias(false, byval_bid);
  }
  ptr_alias.emplace(p.getBid(), std::move(alias));

  return std::move(p).release();
}

pair<expr, expr> Memory::mkUndefInput(const ParamAttrs &attrs0) {
  unsigned bits = bits_for_offset + bits_for_bid - Pointer::hasLocalBit();
  expr undef = expr::mkFreshVar("undef", expr::mkUInt(0, bits));

  auto attrs = attrs0;
  attrs.set(ParamAttrs::IsArg);
  Pointer ptr(*this,
              Pointer::mkLongBid(undef.extract(bits-1, bits_for_offset), false),
              undef.extract(bits_for_offset-1, 0), attrs);

  if (attrs0.has(ParamAttrs::NonNull))
    state->addPre(!ptr.isNull());

  return { std::move(ptr).release(), std::move(undef) };
}

Memory::FnRetData Memory::FnRetData::mkIf(const expr &cond, const FnRetData &a,
                                          const FnRetData &b) {
  return { expr::mkIf(cond, a.size, b.size),
           expr::mkIf(cond, a.align, b.align),
           expr::mkIf(cond, a.var, b.var) };
}

pair<expr,Memory::FnRetData>
Memory::mkFnRet(const char *name0, const vector<PtrInput> &ptr_inputs,
                bool is_local, const FnRetData *data) {
  assert(has_fncall);
  string name = string("#fnret_") + name0;

  // can return local block or null (on address space 0 only FIXME)
  if (is_local) {
    auto bid = next_local_bid++;
    assert(bid < numLocals());

    expr size = data ? data->size
                     : expr::mkFreshVar((name + string("#size")).c_str(),
                                        expr::mkUInt(0, bits_size_t-1)).zext(1);
    expr align = data ? data->align
                      : expr::mkFreshVar((name + string("#align")).c_str(),
                                         expr::mkUInt(0, bitsAlignmentInfo()));

    expr var
      = data ? data->var
             : expr::mkFreshVar(name.c_str(),
                                expr::mkUInt(0, 2 + bits_for_offset));
    auto is_null = var.extract(0, 0) == 0;
    auto alloc_ty = var.extract(1, 1);
    auto offset = var.extract(bits_for_offset+2-1, 2);
    auto allocated = !is_null;

    auto p_bid = expr::mkUInt(bid, Pointer::bitsShortBid());
    p_bid = Pointer::mkLongBid(p_bid, true);
    Pointer ptr(*this, p_bid, offset);

    auto short_bid = ptr.getShortBid();
    mkLocalDisjAddrAxioms(allocated, short_bid, size, align, 0);
    store_bv(ptr, allocated, local_block_liveness, non_local_block_liveness);
    local_blk_size.add(short_bid, expr(size));
    local_blk_align.add(short_bid, expr(align));

    static_assert((Pointer::MALLOC & 2) == 2 && (Pointer::CXX_NEW & 2) == 2);
    local_blk_kind.add(short_bid, expr::mkUInt(1, 1).concat(alloc_ty));

    return { expr::mkIf(is_null, Pointer::mkNullPointer(*this)(), ptr()),
             { std::move(size), std::move(align), std::move(var) } };
  }

  bool has_local = hasEscapedLocals();

  unsigned bits_bid = has_local ? bits_for_bid : Pointer::bitsShortBid();
  expr var = expr::mkFreshVar(name.c_str(),
                              expr::mkUInt(0, bits_bid + bits_for_offset));
  auto p_bid = var.extract(bits_bid + bits_for_offset - 1, bits_for_offset);
  if (!has_local)
    p_bid = Pointer::mkLongBid(p_bid, false);
  Pointer p(*this, p_bid, var.extract(bits_for_offset-1, 0));

  auto bid = p.getShortBid();
  expr local = escaped_local_blks.mayAlias(true, bid);

  unsigned max_nonlocal_bid = nextNonlocalBid();
  expr nonlocal = bid.ule(max_nonlocal_bid);
  auto alias = escaped_local_blks;
  alias.setMayAliasUpTo(false, max_nonlocal_bid);

  for (auto [byval_bid, is_const] : byval_blks) {
    nonlocal &= bid != byval_bid;
    alias.setNoAlias(false, byval_bid);
  }
  ptr_alias.emplace(p.getBid(), std::move(alias));

  state->addAxiom(expr::mkIf(p.isLocal(), local, nonlocal));
  return { std::move(p).release(), {} };
}

expr Memory::CallState::writes(unsigned idx) const {
  return writes_block.extract(idx, idx) == 1;
}

Memory::CallState Memory::CallState::mkIf(const expr &cond,
                                          const CallState &then,
                                          const CallState &els) {
  CallState ret;
  auto then_sz = then.non_local_block_val.size();
  auto else_sz = els.non_local_block_val.size();
  for (unsigned i = 0, e = max(then_sz, else_sz); i != e; ++i) {
    if (i >= then_sz) {
      ret.non_local_block_val.emplace_back(els.non_local_block_val[i]);
    } else if (i >= else_sz) {
      ret.non_local_block_val.emplace_back(then.non_local_block_val[i]);
    } else {
      ret.non_local_block_val.emplace_back(
        mk_block_if(cond, then.non_local_block_val[i],
                    els.non_local_block_val[i]));
    }
  }
  ret.writes_block = expr::mkIf(cond, then.writes_block, els.writes_block);
  ret.frees_block = expr::mkIf(cond, then.frees_block, els.frees_block);
  ret.writes_args = expr::mkIf(cond, then.writes_args, els.writes_args);
  ret.non_local_liveness = expr::mkIf(cond, then.non_local_liveness,
                                      els.non_local_liveness);
  return ret;
}

expr Memory::CallState::operator==(const CallState &rhs) const {
  expr ret = non_local_liveness == rhs.non_local_liveness;
  if (non_local_block_val.size() == rhs.non_local_block_val.size()) {
    for (unsigned i = 0, e = non_local_block_val.size(); i != e; ++i) {
      ret &= non_local_block_val[i] == rhs.non_local_block_val[i];
    }
  }
  ret &= writes_block == rhs.writes_block;
  ret &= frees_block == rhs.frees_block;
  if (writes_args.isValid())
    ret &= writes_args == rhs.writes_args;
  return ret;
}

Memory::CallState
Memory::mkCallState(const string &fnname, bool nofree, unsigned num_ptr_args,
                    const SMTMemoryAccess &access) {
  assert(has_fncall);
  CallState st;
  st.non_local_liveness = mk_liveness_array();
  st.frees_block = expr::mkUInt(0, st.non_local_liveness);

  {
    unsigned num_blocks = 1;
    unsigned limit = num_nonlocals_src - num_inaccessiblememonly_fns;
    for (unsigned i = 0; i < limit; ++i) {
      if (!always_nowrite(i, true, true))
        ++num_blocks;
    }
    st.writes_block = expr::mkUInt(0, num_blocks);
  }

  if (access.canWriteSomething().isFalse()) {
    st.writes_args = expr::mkUInt(0, num_ptr_args);
    return st;
  }

  // TODO: handle havoc of local blocks

  // inaccessible memory block
  unsigned innaccess_bid = num_nonlocals_src - 1;
  st.non_local_block_val.emplace_back(
    expr::mkFreshVar("blk_val", non_local_block_val[innaccess_bid].val));

  expr only_write_inaccess = access.canOnlyWrite(MemoryAccess::Inaccessible);

  if (!only_write_inaccess.isTrue()) {
    unsigned limit = num_nonlocals_src - num_inaccessiblememonly_fns;
    for (unsigned i = 0; i < limit; ++i) {
      if (always_nowrite(i, true, true))
        continue;
      st.non_local_block_val.emplace_back(
        expr::mkFreshVar("blk_val", mk_block_val_array(i)));
    }
    st.writes_block = expr::mkFreshVar("writes_block", st.writes_block);
    assert(st.writes_block.bits() == st.non_local_block_val.size());
  }
  else if (!only_write_inaccess.isFalse()) {
    auto var = expr::mkFreshVar("writes_block", expr::mkUInt(0, 1));
    if (st.writes_block.bits() > 1)
      st.writes_block = expr::mkUInt(0, st.writes_block.bits()-1).concat(var);
    else
      st.writes_block = std::move(var);
  }

  st.writes_args
    = expr::mkFreshVar("writes_args", expr::mkUInt(0, num_ptr_args));

  if (num_nonlocals_src && !nofree) {
    auto may_free = access.canAccess(MemoryAccess::Other) ||
                    access.canAccess(MemoryAccess::Inaccessible);
    st.frees_block
      = mkIf_fold(may_free,
                  expr::mkFreshVar("frees_block", st.non_local_liveness),
                  st.frees_block);
    st.non_local_liveness
      = mkIf_fold(may_free,
                  expr::mkFreshVar("blk_liveness", st.non_local_liveness),
                  st.non_local_liveness);
  }

  for (auto &block : st.non_local_block_val) {
    mkNonlocalValAxioms(block);
  }

  return st;
}

void Memory::setState(const Memory::CallState &st,
                      const SMTMemoryAccess &access,
                      const vector<PtrInput> &ptr_inputs,
                      unsigned inaccessible_bid) {
  assert(has_fncall);

  if (access.canWriteSomething().isFalse())
    return;

  // 1) Havoc memory

  expr only_write_inaccess = access.canOnlyWrite(MemoryAccess::Inaccessible);
  if (!only_write_inaccess.isFalse()) {
    assert(inaccessible_bid != -1u);
    assert(st.non_local_block_val.size() >= 1);
    unsigned bid
      = num_nonlocals_src - num_inaccessiblememonly_fns + inaccessible_bid;
    assert(is_fncall_mem(bid));
    assert(non_local_block_val[bid].undef.empty());
    auto &cur_val = non_local_block_val[bid].val;
    cur_val = mk_block_if(only_write_inaccess && st.writes(0),
                          st.non_local_block_val[0], cur_val);
  }

  // TODO: MemoryAccess::Errno

  if (!only_write_inaccess.isTrue()) {
    unsigned idx = 1;
    unsigned limit = num_nonlocals_src - num_inaccessiblememonly_fns;
    const auto written_blocks = st.non_local_block_val.size();
    for (unsigned bid = 0; bid < limit && idx < written_blocks; ++bid) {
      if (always_nowrite(bid, true, true))
        continue;

      expr modifies = access.canWrite(MemoryAccess::Other) && st.writes(idx);

      if (!is_fncall_mem(bid)) {
        unsigned arg_idx = 0;
        for (auto &ptr_in : ptr_inputs) {
          if (bid < next_nonlocal_bid) {
            expr writes = st.writes_args.extract(arg_idx, arg_idx) == 1;
            expr cond = !ptr_in.nowrite &&
                        ptr_in.byval == 0 &&
                        access.canWrite(MemoryAccess::Args) &&
                        writes;

            Pointer ptr(*this, ptr_in.val.value);
            modifies |= cond &&
                        !ptr.isNull() &&
                        ptr.getBid() == bid;
            state->addUB(cond.implies(ptr_in.val.non_poison));
            state->addUB(ptr_in.nowrite.implies(!writes));
            ++arg_idx;
          }
        }
      }

      auto &cur_val = non_local_block_val[bid].val;
      auto &new_val = st.non_local_block_val[idx++];
      cur_val = mk_block_if(modifies, new_val, std::move(cur_val));
      if (modifies.isTrue())
        non_local_block_val[bid].undef.clear();

      if (!modifies.isFalse()) {
        auto &stores = stored_pointers[bid];
        stores.first.clear();
        stores.second = false;
      }
    }
    assert(written_blocks == 0 || idx == written_blocks);
  }

  stored_pointers[get_fncallmem_bid()].second = true;

  if (!st.non_local_liveness.isAllOnes()) {
    expr one  = expr::mkUInt(1, num_nonlocals);
    expr zero = expr::mkUInt(0, one);
    expr mask = always_nowrite(0) ? one : zero;
    expr may_free = access.canAccess(MemoryAccess::Other) ||
                    access.canAccess(MemoryAccess::Inaccessible);

    for (unsigned bid = always_nowrite(0); bid < num_nonlocals; ++bid) {
      expr heap = Pointer(*this, bid, false).isHeapAllocated();
      mask = mask | expr::mkIf(heap && may_free && !is_fncall_mem(bid) &&
                                 st.frees_block.extract(bid, bid) == 1,
                               zero,
                               one << expr::mkUInt(bid, one));
    }

    // functions can free an object, but cannot bring a dead one back to live
    non_local_block_liveness
      = non_local_block_liveness & (st.non_local_liveness | mask);
  }

  // TODO: function calls can also free local objects passed by argument

  // TODO: havoc local blocks
  // for now, zero out if in non UB-exploitation mode to avoid false positives
  if (config::disallow_ub_exploitation) {
    expr zero_byte = Byte(*this, {expr::mkUInt(0, bits_byte), true}, 0, true)();

    for (unsigned i = 0; i < next_local_bid; ++i) {
      if (escaped_local_blks.mayAlias(true, i)) {
        local_block_val[i] = expr(zero_byte);
      }
    }
  }
}

static expr disjoint_local_blocks(const Memory &m, const expr &addr,
                                  const expr &sz, const expr &align,
                                  const FunctionExpr &blk_addr) {
  expr disj = true;

  // Disjointness of block's address range with other local blocks
  auto zero = expr::mkUInt(0, bits_for_offset);
  for (auto &[sbid, addr0] : blk_addr) {
    Pointer p2(m, Pointer::mkLongBid(sbid, true), zero);
    disj &= p2.isBlockAlive()
              .implies(disjoint(addr, sz, align, p2.getAddress(),
                                p2.blockSizeAligned()
                                  .zextOrTrunc(bits_ptr_address),
                                p2.blockAlignment()));
  }
  return disj;
}

void Memory::mkLocalDisjAddrAxioms(const expr &allocated, const expr &short_bid,
                                   const expr &size, const expr &align,
                                   unsigned align_bits) {
  if (!observesAddresses())
    return;

  unsigned var_bw = bits_ptr_address - align_bits - Pointer::hasLocalBit();
  auto addr_var = expr::mkFreshVar("local_addr", expr::mkUInt(0, var_bw));
  state->addQuantVar(addr_var);

  if (!Pointer::hasLocalBit())
    state->addPre(addr_var != 0);

  expr blk_addr = addr_var.concat_zeros(align_bits);
  expr full_addr = Pointer::hasLocalBit()
                     ? expr::mkUInt(1, 1).concat(blk_addr) : blk_addr;

  if (align_bits == 0) {
    auto shift = expr::mkUInt(bits_ptr_address, full_addr) -
                   align.zextOrTrunc(bits_ptr_address);
    state->addPre((full_addr << shift) == 0);
  }

  // addr + size only overflows for one case when obj is aligned
  expr no_ovfl;
  if (size.ule(align.zextOrTrunc(bits_size_t)).isTrue())
    no_ovfl = addr_var != expr::mkInt(-1, addr_var);
  else
    no_ovfl
      = blk_addr.add_no_uoverflow(
          size.zextOrTrunc(bits_ptr_address - Pointer::hasLocalBit()));
  state->addPre(allocated.implies(no_ovfl));

  // Disjointness of block's address range with other local blocks
  state->addPre(
    allocated.implies(
      disjoint_local_blocks(*this, full_addr,
                            size.zextOrTrunc(bits_ptr_address),
                            align, local_blk_addr)));

  local_blk_addr.add(short_bid, std::move(blk_addr));
}

pair<expr, expr>
Memory::alloc(const expr *size, uint64_t align, BlockKind blockKind,
              const expr &precond, const expr &nonnull,
              optional<unsigned> bidopt, unsigned *bid_out, bool is_function) {
  assert(!memory_unused());

  // Produce a local block if blockKind is heap or stack.
  bool is_local = blockKind != GLOBAL && blockKind != CONSTGLOBAL;
  bool is_const = blockKind == CONSTGLOBAL;

  auto &last_bid = is_local ? next_local_bid
                            : (is_const ? next_const_bid : next_global_bid);
  unsigned bid = bidopt.value_or(last_bid);
  assert((is_local && bid < numLocals()) ||
         (!is_local && bid < numNonlocals()));
  if (!bidopt)
    ++last_bid;
  assert(bid < last_bid);

  if (bid_out)
    *bid_out = bid;

  expr size_zext;
  expr nooverflow = true;
  if (size) {
    size_zext  = size->zextOrTrunc(bits_size_t);
    // we round up the size statically instead of creating a large expr later
    if (!has_globals_diff_align)
      size_zext = size_zext.round_up(expr::mkUInt(align, bits_size_t));
    nooverflow = size->bits() <= bits_size_t ? true :
                   size->extract(size->bits()-1, bits_size_t) == 0;
  }

  expr allocated = precond && nooverflow;
  state->addPre(nonnull.implies(allocated));
  allocated |= nonnull;

  Pointer p(*this, bid, is_local);
  auto short_bid = p.getShortBid();
  // TODO: If address space is not 0, the address can be 0.
  // TODO: add support for C++ allocs
  unsigned alloc_ty = 0;
  switch (blockKind) {
  case MALLOC:      alloc_ty = Pointer::MALLOC; break;
  case CXX_NEW:     alloc_ty = Pointer::CXX_NEW; break;
  case STACK:       alloc_ty = Pointer::STACK; break;
  case GLOBAL:
  case CONSTGLOBAL: alloc_ty = Pointer::GLOBAL; break;
  }

  assert(align != 0);
  auto align_bits = ilog2(align);
  expr align_expr = expr::mkUInt(align_bits, bitsAlignmentInfo());
  bool is_null = !is_local && has_null_block && bid == 0;

  if (is_local) {
    assert(size);
    mkLocalDisjAddrAxioms(allocated, short_bid, size_zext, align_expr,
                          align_bits);
  } else {
    // support for 0-sized arrays like [0 x i8], which are arbitrarily sized
    if (size)
      state->addAxiom(p.blockSize() == size_zext);
    if (is_function)
      state->addAxiom(p.blockSize() != 0);
    state->addAxiom(p.isBlockAligned(align, true));
    state->addAxiom(p.getAllocType() == alloc_ty);

    assert(is_const == is_constglb(bid, state->isSource()));
    assert((has_null_block && bid == 0) ||
           is_globalvar(bid, !state->isSource()));
  }

  if (!is_null)
    store_bv(p, allocated, local_block_liveness, non_local_block_liveness);
  if (size)
    (is_local ? local_blk_size : non_local_blk_size)
      .add(short_bid, std::move(size_zext));
  (is_local ? local_blk_align : non_local_blk_align)
    .add(short_bid, std::move(align_expr));
  (is_local ? local_blk_kind : non_local_blk_kind)
    .add(short_bid, expr::mkUInt(alloc_ty, 2));

  if (Pointer(*this, bid, is_local).isBlkSingleByte()) {
    if (is_local)
      local_block_val[bid].val = Byte::mkPoisonByte(*this)();
    else
      non_local_block_val[bid].val = mk_block_val_array(bid);
  }

  if (!nonnull.isTrue()) {
    expr nondet_nonnull = expr::mkFreshVar("#alloc_nondet_nonnull", true);
    state->addQuantVar(nondet_nonnull);
    allocated = precond && (nonnull || (nooverflow && nondet_nonnull));
  }
  return { std::move(p).release(), std::move(allocated) };
}

void Memory::startLifetime(const expr &ptr_local) {
  assert(!memory_unused());
  Pointer p(*this, ptr_local);
  state->addUB(p.isLocal());

  if (observesAddresses())
    state->addPre(p.isBlockAlive() ||
      disjoint_local_blocks(*this, p.getAddress(),
                            p.blockSizeAligned().zextOrTrunc(bits_ptr_address),
                            p.blockAlignment(), local_blk_addr));

  store_bv(p, true, local_block_liveness, non_local_block_liveness, true);
}

void Memory::free(const expr &ptr, bool unconstrained) {
  assert(!memory_unused());
  Pointer p(*this, ptr);
  expr isnnull = p.isNull();

  if (!unconstrained)
    state->addUB(isnnull || (p.getOffset() == 0 &&
                             p.isBlockAlive() &&
                             p.getAllocType() == Pointer::MALLOC));

  if (!isnnull.isTrue()) {
    // A nonlocal block for encoding fn calls' side effects cannot be freed.
    ensure_non_fncallmem(p);
    store_bv(p, false, local_block_liveness, non_local_block_liveness, false,
             !isnnull);
  }
}

unsigned Memory::getStoreByteSize(const Type &ty) {
  assert(bits_program_pointer != 0);

  if (ty.isPtrType())
    return divide_up(bits_program_pointer, 8);

  auto aty = ty.getAsAggregateType();
  if (aty && !isNonPtrVector(ty)) {
    unsigned sz = 0;
    for (unsigned i = 0, e = aty->numElementsConst(); i < e; ++i)
      sz += getStoreByteSize(aty->getChild(i));
    return sz;
  }
  return divide_up(ty.bits(), 8);
}

void Memory::store(const StateValue &v, const Type &type, unsigned offset0,
                   vector<pair<unsigned, expr>> &data) {
  unsigned bytesz = bits_byte / 8;

  auto aty = type.getAsAggregateType();
  if (aty && !isNonPtrVector(type)) {
    unsigned byteofs = 0;
    for (unsigned i = 0, e = aty->numElementsConst(); i < e; ++i) {
      auto &child = aty->getChild(i);
      if (child.bits() == 0)
        continue;
      store(aty->extract(v, i), child, offset0 + byteofs, data);
      byteofs += getStoreByteSize(child);
    }
    assert(byteofs == getStoreByteSize(type));

  } else {
    vector<Byte> bytes = valueToBytes(v, type, *this, *state);
    assert(!v.isValid() || bytes.size() * bytesz == getStoreByteSize(type));

    for (unsigned i = 0, e = bytes.size(); i < e; ++i) {
      unsigned offset = little_endian ? i * bytesz : (e - i - 1) * bytesz;
      data.emplace_back(offset0 + offset, std::move(bytes[i])());
    }
  }
}

void Memory::store(const expr &p, const StateValue &v, const Type &type,
                   uint64_t align, const set<expr> &undef_vars) {
  assert(!memory_unused());
  Pointer ptr(*this, p);

  // initializer stores are ok by construction
  if (!state->isInitializationPhase())
    state->addUB(ptr.isDereferenceable(getStoreByteSize(type), align, true));

  vector<pair<unsigned, expr>> to_store;
  store(v, type, 0, to_store);
  store(ptr, to_store, undef_vars, align);
}

StateValue Memory::load(const Pointer &ptr, const Type &type, set<expr> &undef,
                        uint64_t align) {
  unsigned bytecount = getStoreByteSize(type);

  auto aty = type.getAsAggregateType();
  if (aty && !isNonPtrVector(type)) {
    vector<StateValue> member_vals;
    unsigned byteofs = 0;
    for (unsigned i = 0, e = aty->numElementsConst(); i < e; ++i) {
      // Padding is filled with poison.
      if (aty->isPadding(i)) {
        byteofs += getStoreByteSize(aty->getChild(i));
        continue;
      }

      auto ptr_i = ptr + byteofs;
      auto align_i = gcd(align, byteofs % align);
      member_vals.emplace_back(load(ptr_i, aty->getChild(i), undef, align_i));
      byteofs += getStoreByteSize(aty->getChild(i));
    }
    assert(byteofs == bytecount);
    return aty->aggregateVals(member_vals);
  }

  bool is_ptr = type.isPtrType();
  auto loadedBytes = load(ptr, bytecount, undef, align, little_endian,
                          is_ptr ? DATA_PTR : DATA_INT);
  auto val = bytesToValue(*this, loadedBytes, type);

  // partial order reduction for fresh pointers
  // can alias [0, next_ptr++] U extra_tgt_consts
  // Note that if we reached the max number of bids, it's pointless to
  // remember that the pointer must be within [0, max], so skip this code
  // in that case to save memory.
  if (is_ptr && !val.non_poison.isFalse() &&
      next_nonlocal_bid <= max_program_nonlocal_bid()) {
    optional<unsigned> max_bid;
    for (auto &p : all_leaf_ptrs(*this, val.value)) {
      auto islocal = p.isLocal();
      auto bid = p.getShortBid();
      if (!islocal.isTrue() && !bid.isConst()) {
        auto [I, inserted] = ptr_alias.try_emplace(p.getBid(), *this);
        if (inserted) {
          if (!max_bid)
            max_bid = nextNonlocalBid();
          I->second.setMayAliasUpTo(false, *max_bid);
          for (unsigned i = num_nonlocals_src; i < numNonlocals(); ++i) {
            I->second.setMayAlias(false, i);
          }
          state->addPre(!val.non_poison || islocal || bid.ule(*max_bid));
        }
      }
    }
  }

  return val;
}

pair<StateValue, pair<AndExpr, expr>>
Memory::load(const expr &p, const Type &type, uint64_t align) {
  assert(!memory_unused());

  Pointer ptr(*this, p);
  auto ubs = ptr.isDereferenceable(getStoreByteSize(type), align, false);
  set<expr> undef_vars;
  auto ret = load(ptr, type, undef_vars, align);
  return { state->rewriteUndef(std::move(ret), undef_vars), std::move(ubs) };
}

Byte Memory::raw_load(const Pointer &p, set<expr> &undef) {
  return std::move(load(p, bits_byte / 8, undef, 1)[0]);
}

Byte Memory::raw_load(const Pointer &p) {
  set<expr> undef;
  return { *this, state->rewriteUndef(raw_load(p, undef)(), undef) };
}

void Memory::memset(const expr &p, const StateValue &val, const expr &bytesize,
                    uint64_t align, const set<expr> &undef_vars,
                    bool deref_check) {
  assert(!memory_unused());
  assert(!val.isValid() || val.bits() == 8);
  unsigned bytesz = bits_byte / 8;
  Pointer ptr(*this, p);
  if (deref_check)
    state->addUB(ptr.isDereferenceable(bytesize, align, true, false, false));

  auto wval = val;
  for (unsigned i = 1; i < bytesz; ++i) {
    wval = wval.concat(val);
  }
  assert(!val.isValid() || wval.bits() == bits_byte);

  auto bytes = valueToBytes(wval, IntType("", bits_byte), *this, *state);
  assert(bytes.size() == 1);
  expr raw_byte = std::move(bytes[0])();

  uint64_t n;
  if (bytesize.isUInt(n) && (n / bytesz) <= 4) {
    vector<pair<unsigned, expr>> to_store;
    for (unsigned i = 0; i < n; i += bytesz) {
      to_store.emplace_back(i, raw_byte);
    }
    store(ptr, to_store, undef_vars, align);
  } else {
    expr offset = expr::mkQVar(0, Pointer::bitsShortOffset());
    storeLambda(ptr, offset, bytesize, {{0, raw_byte}}, undef_vars, align);
  }
}

void Memory::memset_pattern(const expr &ptr0, const expr &pattern0,
                            const expr &bytesize, unsigned pattern_length) {
  assert(!memory_unused());
  unsigned bytesz = bits_byte / 8;
  Pointer ptr(*this, ptr0);
  state->addUB(ptr.isDereferenceable(bytesize, 1, true));

  Pointer pattern(*this, pattern0);
  expr length = bytesize.umin(expr::mkUInt(pattern_length, bytesize));
  state->addUB(pattern.isDereferenceable(length, 1, false));

  set<expr> undef_vars;
  auto bytes
    = load(pattern, pattern_length, undef_vars, 1, little_endian, DATA_ANY);

  vector<pair<unsigned, expr>> to_store;
  uint64_t n;
  if (bytesize.isUInt(n) && (n / bytesz) <= 4) {
    for (unsigned i = 0; i < n; i += bytesz) {
      to_store.emplace_back(i, std::move(bytes[i/bytesz])());
    }
    store(ptr, to_store, undef_vars, 1);
  } else {
    assert(bytes.size() * bytesz == pattern_length);
    for (unsigned i = 0; i < pattern_length; i += bytesz) {
      to_store.emplace_back(i * bytesz, std::move(bytes[i/bytesz])());
    }
    expr offset = expr::mkQVar(0, Pointer::bitsShortOffset());
    storeLambda(ptr, offset, bytesize, to_store, undef_vars, 1);
  }
}

void Memory::memcpy(const expr &d, const expr &s, const expr &bytesize,
                    uint64_t align_dst, uint64_t align_src, bool is_move) {
  assert(!memory_unused());
  unsigned bytesz = bits_byte / 8;

  Pointer dst(*this, d), src(*this, s);
  state->addUB(dst.isDereferenceable(bytesize, align_dst, true, false, false));
  state->addUB(src.isDereferenceable(bytesize, align_src, false, false, false));
  if (!is_move)
    src.isDisjointOrEqual(bytesize, dst, bytesize);

  // copy to itself
  if ((src == dst).isTrue())
    return;

  uint64_t n;
  if (bytesize.isUInt(n) && (n / bytesz) <= 4) {
    vector<pair<unsigned, expr>> to_store;
    set<expr> undef;
    unsigned i = 0;
    for (auto &byte : load(src, n, undef, align_src)) {
      to_store.emplace_back(i++ * bytesz, std::move(byte)());
    }
    store(dst, to_store, undef, align_dst);
  } else {
    expr offset = expr::mkQVar(0, Pointer::bitsShortOffset());
    Pointer ptr_src = src + (offset - dst.getShortOffset());
    set<expr> undef;
    storeLambda(dst, offset, bytesize, {{0, raw_load(ptr_src, undef)()}}, undef,
                align_dst);
  }
}

void Memory::copy(const Pointer &src, const Pointer &dst) {
  auto local = dst.isLocal();
  if (!local.isValid()) {
    local_block_val.clear();
    non_local_block_val.clear();
    local_block_val.resize(numLocals());
    non_local_block_val.resize(numNonlocals());
    return;
  }

  assert(local.isConst());
  bool dst_local = local.isTrue();
  uint64_t dst_bid;
  expr dst_bid_expr = dst.getShortBid();
  ENSURE(dst_bid_expr.isUInt(dst_bid));
  auto &dst_blk = (dst_local ? local_block_val : non_local_block_val)[dst_bid];
  dst_blk.undef.clear();
  dst_blk.type = DATA_NONE;

  unsigned has_bv_val = 0;
  auto offset = expr::mkUInt(0, Pointer::bitsShortOffset());
  DisjointExpr val(Byte::mkPoisonByte(*this)());

  if (!dst_local)
    record_stored_pointer(dst_bid, offset);

  auto fn = [&](MemBlock &blk, const Pointer &ptr, unsigned src_bid,
                bool src_local, expr &&cond) {
    // we assume src != dst
    if (src_local == dst_local && src_bid == dst_bid)
      return;
    val.add(blk.val, std::move(cond));
    dst_blk.undef.insert(blk.undef.begin(), blk.undef.end());
    dst_blk.type |= blk.type;
    has_bv_val   |= 1u << blk.val.isBV();
  };
  access(src, expr::mkUInt(bits_byte/8, bits_size_t), bits_byte/8, false, fn);

  // if we have mixed array/non-array blocks, switch them all to array
  if (has_bv_val == 3) {
    DisjointExpr<expr> newval;
    for (auto &[v, cond] : val) {
      newval.add(v.isBV() ? expr::mkConstArray(offset, v) : v, cond);
    }
    val = std::move(newval);
  }
  dst_blk.val = *std::move(val)();
}

void Memory::fillPoison(const expr &bid) {
  Pointer p(*this, bid, expr::mkUInt(0, bits_for_offset));
  expr blksz = p.blockSizeAligned();
  memset(std::move(p).release(), IntType("i8", 8).getDummyValue(false),
         std::move(blksz), bits_byte / 8, {}, false);
}

expr Memory::ptr2int(const expr &ptr) {
  assert(!memory_unused() && observesAddresses());
  Pointer p(*this, ptr);
  observesAddr(p);
  state->addUB(!p.isNocapture());
  return p.getAddress();
}

expr Memory::int2ptr(const expr &val) {
  assert(!memory_unused() && has_int2ptr && observesAddresses());
  nextNonlocalBid();
  return
    Pointer::mkPhysical(*this, val.zextOrTrunc(bits_ptr_address)).release();
}

expr Memory::blockRefined(const Pointer &src, const Pointer &tgt) const {
  expr aligned(true);
  expr src_align = src.blockAlignment();
  expr tgt_align = tgt.blockAlignment();
  // if they are both non-const, then the condition holds per the precondition
  if (src_align.isConst() || tgt_align.isConst())
    aligned = src_align.ule(tgt_align);

  return src.isBlockAlive() == tgt.isBlockAlive() &&
         src.blockSize()    == tgt.blockSize() &&
         src.getAllocType() == tgt.getAllocType() &&
         aligned;
}

expr Memory::blockValRefined(const Pointer &src, const Memory &tgt,
                             unsigned bid, set<expr> &undef,
                             bool full_check) const {
  auto &mem1 = non_local_block_val[bid];
  auto &mem2 = tgt.non_local_block_val[bid].val;

  if (mem1.val.eq(mem2))
    return true;

  // must have had called the exact same function before
  if (is_fncall_mem(bid))
    return mem1.val == mem2;

  auto refined = [&](const expr &offset) -> expr {
    int is_fn1 = isInitialMemBlock(mem1.val, true);
    int is_fn2 = isInitialMemBlock(mem2, true);
    if (is_fn1 && is_fn2) {
      // if both memories are the result of a function call, then refinement
      // holds iff they are equal, otherwise we can always force a behavior
      // of the function such that it will store a different value to memory
      if (is_fn1 == 2 && is_fn2 == 2)
        return mem1.val == mem2;

      // an initial memory (m0) vs a function call is always false, as a function
      // may always store something to memory
      assert((is_fn1 == 1 && is_fn2 == 2) || (is_fn1 == 2 && is_fn2 == 1));
      return false;
    }

    Byte val  = raw_load(false, bid, offset);
    Byte val2 = tgt.raw_load(false, bid, offset);

    if (val.eq(val2))
      return true;

    undef.insert(mem1.undef.begin(), mem1.undef.end());

    expr r = val.refined(val2);

    // The ABI requires that sub-byte stores zero extend the stored value
    if (num_sub_byte_bits && tgt.isAsmMode())
      r &= mkSubByteZExtStoreCond(val, val2);

    return r;
  };

  unsigned bytes_per_byte = bits_byte / 8;
  expr ptr_offset = src.getShortOffset();
  uint64_t bytes;
  if (full_check &&
      src.blockSize().isUInt(bytes) && (bytes / bytes_per_byte) <= 8) {
    expr val_refines = true;
    for (unsigned off = 0; off < (bytes / bytes_per_byte); ++off) {
      expr off_expr = expr::mkUInt(off, ptr_offset);
      val_refines &= (ptr_offset == off_expr).implies(refined(off_expr));
    }
    return val_refines;
  } else {
    expr cnstr = refined(ptr_offset);
    if (!full_check)
      return cnstr;
    return src.getOffsetSizet().ult(src.blockSizeOffsetT()).implies(cnstr);
  }
}

tuple<expr, Pointer, set<expr>>
Memory::refined(const Memory &other, bool fncall,
                const vector<PtrInput> *set_ptrs,
                const vector<PtrInput> *set_ptrs2) const {
  if (num_nonlocals == 0)
    return { true, Pointer(*this, expr()), {} };

  assert(!memory_unused());
  Pointer ptr(*this, "#idx_refinement", false);
  expr ptr_bid = ptr.getBid();
  expr offset = ptr.getOffset();
  expr ret(true);
  set<expr> undef_vars;

  AliasSet block_alias(*this, other);
  auto min_read_sz = bits_byte / 8;
  expr min_read_sz_expr = expr::mkUInt(min_read_sz, bits_size_t);

  auto sets = { make_pair(this, set_ptrs), make_pair(&other, set_ptrs2) };
  for (const auto &[mem, set] : sets) {
    if (set) {
      TmpValueChange tmp(next_local_bid, mem->numLocals());
      for (auto &it: *set_ptrs) {
        block_alias.unionWith(
          mem->computeAliasing(Pointer(*mem, it.val.value), min_read_sz_expr,
                               min_read_sz, false));
      }
    } else {
      if (mem->next_nonlocal_bid > 0)
        block_alias.setMayAliasUpTo(false, min(mem->next_nonlocal_bid,
                                               num_nonlocals)-1);

      if (has_write_fncall)
        block_alias.setMayAlias(false, get_fncallmem_bid());

      if (!fncall) {
        for (unsigned bid = num_nonlocals_src - num_inaccessiblememonly_fns;
             bid < num_nonlocals_src; ++bid) {
          block_alias.setMayAlias(false, bid);
        }
      }
    }
  }

  for (unsigned bid = 0; bid < num_nonlocals_src; ++bid) {
    if ((fncall || bid == 0) && always_nowrite(bid, true, true))
      continue;

    if (!block_alias.mayAlias(false, bid))
      continue;

    // Constants that are not referenced can be removed.
    if (is_constglb(bid) && !other.state->isGVUsed(bid))
      continue;

    expr bid_expr = expr::mkUInt(bid, ptr_bid);
    Pointer p(*this, bid_expr, offset);
    Pointer q(other, p());
    if (p.isByval().isTrue() && q.isByval().isTrue())
      continue;

    expr val_refined = true;
    uint64_t bytes;

    auto &stored_src = stored_pointers[bid];
    auto &stored_tgt = other.stored_pointers[bid];
    if (!stored_src.second && stored_src.first.empty() &&
        !stored_tgt.second && stored_tgt.first.empty()) {
      // block not stored; no need to verify
    }
    else if (p.blockSize().isUInt(bytes) && (bytes / (bits_byte / 8)) <= 8) {
      // this is a small block; just check it thoroughly
      val_refined = blockValRefined(p, other, bid, undef_vars, true);
    }
    else {
      // else check only the stored offsets
      auto offsets = stored_src.first;
      offsets.insert(stored_tgt.first.begin(), stored_tgt.first.end());
      bool full_check = false;
      if (stored_src.second ||
          stored_tgt.second ||
          offsets.size() > MAX_STORED_PTRS_SET) {
        offsets = { offset };
        full_check = true;
      }

      for (auto &off0 : offsets) {
        auto off = off0.concat_zeros(bits_for_offset - off0.bits());
        Pointer p(*this, bid_expr, off);
        val_refined
          &= (offset == off)
               .implies(blockValRefined(p, other, bid, undef_vars, full_check));
      }
    }
    ret &= (ptr_bid == bid_expr).implies(
             blockRefined(p, q) &&
             p.isBlockAlive().implies(val_refined));
  }

  // restrict refinement check to set of request blocks
  if (set_ptrs) {
    OrExpr c;
    for (auto set : {set_ptrs, set_ptrs2}) {
      for (auto &it: *set) {
        auto &ptr = it.val;
        c.add(ptr.non_poison && Pointer(*this, ptr.value).getBid() == ptr_bid);
      }
    }
    ret = c().implies(ret);
  }

  // TODO: missing refinement of escaped local blocks!

  return { std::move(ret), std::move(ptr), std::move(undef_vars) };
}

expr Memory::returnChecks() const {
  return checkNocapture() &&
         checkInitializes();
}

expr Memory::checkNocapture() const {
  if (!does_ptr_store || !has_nocapture)
    return true;

  auto name = local_name(state, "#offset_nocapture");
  auto offset = expr::mkVar(name.c_str(), Pointer::bitsShortOffset());
  expr res(true);

  for (unsigned bid = 0; bid < numNonlocals(); ++bid) {
    if (always_nowrite(bid))
      continue;
    Pointer p(*this, bid, false);
    Byte b = raw_load(false, bid, offset);
    Pointer loadp(*this, b.ptrValue());
    res &= (p.isBlockAlive() && b.isPtr() && b.ptrNonpoison())
             .implies(!loadp.isNocapture());
  }
  if (!res.isTrue())
    state->addQuantVar(offset);
  return res;
}

expr Memory::checkInitializes() const {
  if (!has_initializes_attr)
    return true;

  expr ret = true;

  for (auto &input0 : state->getFn().getInputs()) {
    auto &input = static_cast<const Input&>(input0);
    auto &inits = input.getAttributes().initializes;
    if (inits.empty())
      continue;

    Pointer arg(*this, (*state)[input].value);
    for (auto [l, h] : inits) {
      ret &= hasStored(arg + l, expr::mkUInt(h - l, bits_size_t));
    }
  }
  return ret;
}

void Memory::record_stored_pointer(uint64_t bid, const expr &offset) {
  auto &set = stored_pointers[bid];
  if (set.second)
    return;
  set.first.emplace(offset);
  if (set.first.size() > MAX_STORED_PTRS_SET) {
    set.first.clear();
    set.second = true;
  }
}

void Memory::escape_helper(const expr &ptr, AliasSet &set1, AliasSet *set2) {
  assert(observesAddresses());

  if (next_local_bid == 0 ||
      set1.isFullUpToAlias(true) == (int)next_local_bid-1)
    return;

  // If we have a physical pointer, only observed addresses can escape
  if (has_int2ptr) {
    Pointer p(*this, ptr);
    if (p.isLogical().isFalse()) {
      if (set2)
        set1.unionWith(*set2);
      return;
    }
  }

  uint64_t bid;
  for (const auto &bid_expr : extract_possible_local_bids(*this, ptr)) {
    if (bid_expr.isUInt(bid)) {
      if (bid < next_local_bid) {
        set1.setMayAlias(true, bid);
        if (set2)
          set2->setMayAlias(true, bid);
      }
    } else if (isInitialMemBlock(bid_expr, true)) {
      // initial non local block bytes don't contain local pointers.
    } else if (isFnReturnValue(bid_expr)) {
      // Function calls have already escaped whatever they needed to.
    } else {
      if (isDerivedFromLoad(bid_expr)) {
        // if this a load, it can't escape anything that hasn't escaped before
        continue;
      }

      // may escape a local ptr, but we don't know which one
      set1.setMayAliasUpTo(true, next_local_bid-1);
      if (set2)
        set2->setMayAliasUpTo(true, next_local_bid-1);
      break;
    }
  }
}

void Memory::escapeLocalPtr(const expr &ptr, const expr &is_ptr) {
  if (is_ptr.isFalse())
    return;

  escape_helper(ptr, escaped_local_blks, &observed_addrs);
}

void Memory::observesAddr(const Pointer &ptr) {
  escape_helper(ptr(), observed_addrs);
}

Memory Memory::mkIf(const expr &cond, Memory &&then, Memory &&els) {
  assert(then.state == els.state);
  Memory &ret = then;
  for (unsigned bid = 0, end = ret.numNonlocals(); bid < end; ++bid) {
    if (always_nowrite(bid, false, true))
      continue;
    auto &blk   = ret.non_local_block_val[bid];
    auto &other = els.non_local_block_val[bid];
    blk.val     = mk_block_if(cond, blk.val, other.val);
    blk.type   |= other.type;
    blk.undef.insert(other.undef.begin(), other.undef.end());
  }
  for (unsigned bid = 0, end = ret.numLocals(); bid < end; ++bid) {
    auto &blk   = ret.local_block_val[bid];
    auto &other = els.local_block_val[bid];
    blk.val     = mk_block_if(cond, blk.val, other.val);
    blk.type   |= other.type;
    blk.undef.insert(other.undef.begin(), other.undef.end());
  }
  ret.non_local_block_liveness = expr::mkIf(cond, then.non_local_block_liveness,
                                            els.non_local_block_liveness);
  ret.local_block_liveness     = expr::mkIf(cond, then.local_block_liveness,
                                            els.local_block_liveness);
  ret.has_stored_arg           = expr::mkIf(cond, then.has_stored_arg,
                                            els.has_stored_arg);
  for (size_t i = 0, e = ret.stored_pointers.size(); i != e; ++i) {
    auto &set1 = ret.stored_pointers[i];
    auto &set2 = els.stored_pointers[i];
    set1.first.insert(set2.first.begin(), set2.first.end());
    set1.second |= set2.second;
    if (set1.second || set1.first.size() > MAX_STORED_PTRS_SET) {
      set1.first.clear();
      set1.second = true;
    }
  }
  ret.local_blk_addr.add(els.local_blk_addr);
  ret.local_blk_size.add(els.local_blk_size);
  ret.local_blk_align.add(els.local_blk_align);
  ret.local_blk_kind.add(els.local_blk_kind);
  ret.non_local_blk_size.add(els.non_local_blk_size);
  ret.non_local_blk_align.add(els.non_local_blk_align);
  ret.non_local_blk_kind.add(els.non_local_blk_kind);
  ret.escaped_local_blks.unionWith(els.escaped_local_blks);
  ret.observed_addrs.unionWith(els.observed_addrs);

  for (const auto &[expr, alias] : els.ptr_alias) {
    auto [I, inserted] = ret.ptr_alias.try_emplace(expr, alias);
    if (!inserted)
      I->second.unionWith(alias);
  }

  ret.next_nonlocal_bid = max(then.next_nonlocal_bid, els.next_nonlocal_bid);
  return std::move(ret);
}

#define P(name, expr) do {      \
  auto v = m.eval(expr, false); \
  uint64_t n;                   \
  if (v.isUInt(n))              \
    os << "\t" name ": " << n;  \
  else if (v.isConst())         \
    os << "\t" name ": " << v;  \
  } while(0)

void Memory::print_array(ostream &os, const expr &a, unsigned indent) const {
  expr idx, val, a2, cond, then, els;
  if (a.isConstArray(val)) {
    os << "*: " << Byte(*this, std::move(val)) << '\n';
  }
  else if (a.isStore(a2, idx, val)) {
    idx.printUnsigned(os);
    os << ": " << Byte(*this, std::move(val)) << '\n';
    print_array(os, a2);
  }
  else if (a.isLambda(val)) {
    print_array(os, val.subst_var(expr::mkVar("idx", a.lambdaIdxType())));
  }
  else if (a.isBV() && a.isConst() && a.bits() == Byte::bitsByte()) {
    os << Byte(*this, expr(a)) << '\n';
  }
  else if (a.isIf(cond, then, els)) {
    auto print_spaces = [&](unsigned limit) {
      for (unsigned i = 0; i < limit; ++i) {
        os << "  ";
      }
    };
    os << "if " << cond << '\n';
    print_spaces(indent + 1);
    print_array(os, then, indent + 1);
    print_spaces(indent);
    os << "else\n";
    print_spaces(indent + 1);
    print_array(os, els, indent + 1);
  }
  else {
    os << a << '\n';
  }
}

void Memory::print(ostream &os, const Model &m) const {
  if (memory_unused())
    return;

  auto print = [&](bool local, unsigned num_bids) {
    for (unsigned bid = 0; bid < num_bids; ++bid) {
      Pointer p(*this, bid, local);
      uint64_t n;
      ENSURE(p.getBid().isUInt(n));
      os << "Block " << n << " >";
      P("size", p.blockSize());
      P("align", expr::mkInt(1, 64) << p.blockAlignment().zextOrTrunc(64));
      P("alloc type", p.getAllocType());
      P("alive", p.isBlockAlive());
      if (observesAddresses())
        P("address", p.getAddress());
      if (!local && is_constglb(bid))
        os << "\tconst";
      os << '\n';

      if (!local && bid < numNonlocals()) {
        expr array = m[mk_block_val_array(bid)].simplify();
        // don't bother with all-poison blocks
        expr val;
        if (array.isConstArray(val) &&
            Byte(*this, std::move(val)).isPoison().isTrue())
          continue;

        os << "Contents:\n";
        print_array(os, array);
        os << '\n';
      }
    }
  };

  bool did_header = false;
  auto header = [&](const char *msg) {
    if (!did_header) {
      os << (state->isSource() ? "\nSOURCE" : "\nTARGET")
         << " MEMORY STATE\n===================\n";
    } else
      os << '\n';
    os << msg;
    did_header = true;
  };

  if (state->isSource() && num_nonlocals) {
    header("NON-LOCAL BLOCKS:\n");
    print(false, num_nonlocals);
  }

  if (numLocals()) {
    header("LOCAL BLOCKS:\n");
    print(true, numLocals());
  }
}
#undef P

#define P(msg, local, nonlocal)                                                \
  os << msg "\n";                                                              \
  if (m.numLocals() > 0) os << "Local: " << m.local.simplify() << '\n';        \
  if (m.numNonlocals() > 0)                                                    \
    os << "Non-local: " << m.nonlocal.simplify() << "\n\n"

ostream& operator<<(ostream &os, const Memory &m) {
  if (memory_unused())
    return os;
  os << "\n\nMEMORY\n======\n"
        "BLOCK VALUE:";
  for (unsigned i = 0; i < m.numLocals(); ++i)
    os << "\nLocal BLK " << i << ":\t" << m.local_block_val[i].val.simplify();
  for (unsigned i = 0; i < m.numNonlocals(); ++i)
    os << "\nNonLocal BLK " << i << ":\t"
       << m.non_local_block_val[i].val.simplify();
  os << '\n';
  P("BLOCK LIVENESS:", local_block_liveness, non_local_block_liveness);
  P("BLOCK SIZE:", local_blk_size, non_local_blk_size);
  P("BLOCK ALIGN:", local_blk_align, non_local_blk_align);
  P("BLOCK KIND:", local_blk_kind, non_local_blk_kind);
  if (m.escaped_local_blks.numMayAlias(true) > 0) {
    m.escaped_local_blks.print(os << "ESCAPED LOCAL BLOCKS: ");
  }
  if (!m.local_blk_addr.empty()) {
    os << "\nLOCAL BLOCK ADDR: " << m.local_blk_addr << '\n';
  }
  os << "\nSTORED ADDRS: " << m.has_stored_arg << '\n';
  if (!m.ptr_alias.empty()) {
    os << "\nALIAS SETS:\n";
    for (auto &[bid, alias] : m.ptr_alias) {
      os << bid << ": ";
      alias.print(os);
      os << '\n';
    }
  }
  os << "\nSTORED NON-LOCAL PTRS:\n";
  for (unsigned i = 0,e = m.stored_pointers.size(); i != e; ++i) {
    os << i << ':';
    for (auto &p : m.stored_pointers[i].first) {
      os << ' ' << p;
    }
    os << " / " << m.stored_pointers[i].second << '\n';
  }
  return os;
}

}
