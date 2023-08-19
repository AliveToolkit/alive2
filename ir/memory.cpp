// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/memory.h"
#include "ir/globals.h"
#include "ir/state.h"
#include "ir/value.h"
#include "smt/solver.h"
#include "util/compiler.h"
#include "util/config.h"
#include <array>
#include <numeric>
#include <string>

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
  assert(has_write_fncall);
  return num_nonlocals_src - num_inaccessiblememonly_fns - 1;
}

static bool is_fncall_mem(unsigned bid) {
  if (!has_write_fncall)
    return false;
  return bid == get_fncallmem_bid();
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


static unsigned next_local_bid;
static unsigned next_const_bid;
static unsigned next_global_bid;
static unsigned next_ptr_input;

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
    Byte::bitsByte() - byte_has_ptr_bit() - bits_byte - bits_poison_per_byte;
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
    auto one = expr::mkUInt(1, bw) << bid;
    auto full = expr::mkInt(-1, bid);
    auto mask = (full << (bid + expr::mkUInt(1, bw))) |
                full.lshr(expr::mkUInt(bw, bw) - bid);
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

Byte::Byte(const Memory &m, const StateValue &v) : m(m) {
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
  p = concat_if(p, np.concat(v.value).concat_zeros(padding_nonptr_byte()));
  assert(!p.isValid() || p.bits() == bitsByte());
}

Byte Byte::mkPoisonByte(const Memory &m) {
  return { m, StateValue(expr::mkUInt(0, bits_byte), false) };
}

expr Byte::isPtr() const {
  return p.sign() == 1;
}

expr Byte::ptrNonpoison() const {
  auto bit = p.bits() - 1 - byte_has_ptr_bit();
  return p.extract(bit, bit) == 1;
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
  unsigned start = padding_nonptr_byte() + bits_byte;
  return p.extract(start + bits_poison_per_byte - 1, start);
}

expr Byte::nonptrValue() const {
  if (!does_int_mem_access)
    return expr::mkUInt(0, bits_byte);
  unsigned start = padding_nonptr_byte();
  return p.extract(start + bits_byte - 1, start);
}

expr Byte::isPoison() const {
  if (!does_int_mem_access)
    return !ptrNonpoison();

  expr np = nonptrNonpoison();
  if (byte_has_ptr_bit() && bits_poison_per_byte == 1) {
    assert(!np.isValid() || ptrNonpoison().eq(np == 1));
    return np == 0;
  }
  return expr::mkIf(isPtr(), !ptrNonpoison(), np != expr::mkInt(-1, np));
}

expr Byte::isZero() const {
  return expr::mkIf(isPtr(), ptr().isNull(), nonptrValue() == 0);
}

expr Byte::refined(const Byte &other) const {
  if (eq(other))
    return true;

  // refinement if offset had non-ptr value
  expr v1 = nonptrValue();
  expr v2 = other.nonptrValue();
  expr np1 = nonptrNonpoison();
  expr np2 = other.nonptrNonpoison();

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

  // fast path: if we didn't do any ptr store, then all ptrs in memory were
  // already there and don't need checking
  expr is_ptr = isPtr();
  expr is_ptr2 = other.isPtr();
  expr ptr_cnstr;
  if (!does_ptr_store || is_ptr.isFalse() || is_ptr2.isFalse()) {
    ptr_cnstr = *this == other;
  } else {
    ptr_cnstr = !ptrNonpoison() ||
                  (other.ptrNonpoison() &&
                   ptrByteoffset() == other.ptrByteoffset() &&
                   ptr().refined(other.ptr()));
  }
  return expr::mkIf(is_ptr == is_ptr2,
                    expr::mkIf(is_ptr, ptr_cnstr, int_cnstr),
                    // allow null ptr <-> zero
                    isPoison() ||
                      (isZero() && !other.isPoison() && other.isZero()));
}

unsigned Byte::bitsByte() {
  unsigned ptr_bits = does_ptr_mem_access *
                        (1 + Pointer::totalBits() + bits_ptr_byte_offset());
  unsigned int_bits = does_int_mem_access * (bits_byte + bits_poison_per_byte);
  // allow at least 1 bit if there's no memory access
  return max(1u, byte_has_ptr_bit() + max(ptr_bits, int_bits));
}

ostream& operator<<(ostream &os, const Byte &byte) {
  if (byte.isPtr().isTrue()) {
    if (byte.ptrNonpoison().isTrue()) {
      os << byte.ptr() << ", byte offset=";
      byte.ptrByteoffset().printSigned(os);
    } else {
      os << "poison";
    }
  } else {
    auto np = byte.nonptrNonpoison();
    auto val = byte.nonptrValue();
    if (np.isAllOnes()) {
      val.printHexadecimal(os);
    } else if (np.isZero()) {
      os << "poison";
    } else {
      os << "#b";
      for (unsigned i = 0; i < bits_poison_per_byte; ++i) {
        unsigned idx = bits_poison_per_byte - i - 1;
        auto is_poison = np.extract(idx, idx).isZero();
        auto v = val.extract(idx, idx).isAllOnes();
        os << (is_poison ? 'p' : (v ? '1' : '0'));
      }
    }
  }
  return os;
}

bool Memory::observesAddresses() {
  return true; //observes_addresses;
}

static bool isFnReturnValue(const expr &e) {
  string name;
  expr val;
  unsigned hi, lo;
  if (e.isExtract(val, hi, lo))
    name = val.fn_name();
  else
    name = e.fn_name();
  return name.starts_with("#fnret_");
}

int Memory::isInitialMemBlock(const expr &e, bool match_any_init) {
  string name;
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
                                 const Memory &mem, State *s) {
  vector<Byte> bytes;
  if (fromType.isPtrType()) {
    Pointer p(mem, val.value);
    unsigned bytesize = bits_program_pointer / bits_byte;

    // constant global can't store pointers that alias with local blocks
    if (s->isInitializationPhase() && !p.isLocal().isFalse()) {
      expr bid  = expr::mkUInt(0, 1).concat(p.getShortBid());
      expr off  = p.getOffset();
      expr attr = p.getAttrs();
      p.~Pointer();
      new (&p) Pointer(mem, bid, off, attr);
    }

    for (unsigned i = 0; i < bytesize; ++i)
      bytes.emplace_back(mem, StateValue(expr(p()), expr(val.non_poison)), i);
  } else {
    assert(!fromType.isAggregateType() || isNonPtrVector(fromType));
    StateValue bvval = fromType.toInt(*s, val);
    unsigned bitsize = bvval.bits();
    unsigned bytesize = divide_up(bitsize, bits_byte);

    pad(bvval, bytesize * bits_byte - bitsize, *s);
    unsigned np_mul = bits_poison_per_byte;

    for (unsigned i = 0; i < bytesize; ++i) {
      StateValue data {
        bvval.value.extract((i + 1) * bits_byte - 1, i * bits_byte),
        bvval.non_poison.extract((i + 1) * np_mul - 1, i * np_mul)
      };
      bytes.emplace_back(mem, data);
    }
  }
  return bytes;
}

static StateValue bytesToValue(const Memory &m, const vector<Byte> &bytes,
                               const Type &toType) {
  assert(!bytes.empty());

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
        non_poison &= is_ptr == b_is_ptr;
      }
      non_poison &=
        expr::mkIf(is_ptr,
                   b.ptrByteoffset() == i && ptr_value == loaded_ptr,
                   b.nonptrValue() == 0);
      non_poison &= !b.isPoison();
    }

    // if bits of loaded ptr are a subset of the non-ptr value,
    // we know they must be zero otherwise the value is poison.
    // Therefore we obtain a null pointer for free.
    expr _, value;
    unsigned low, high, low2, high2;
    if (loaded_ptr.isExtract(_, high, low) &&
        bytes[0].nonptrValue().isExtract(_, high2, low2) &&
        high2 >= high && low2 <= low) {
      value = std::move(loaded_ptr);
    } else {
      value = expr::mkIf(is_ptr, loaded_ptr, Pointer::mkNullPointer(m)());
    }
    return { std::move(value), std::move(non_poison) };

  } else {
    assert(!toType.isAggregateType() || isNonPtrVector(toType));
    auto bitsize = toType.bits();
    assert(divide_up(bitsize, bits_byte) == bytes.size());

    StateValue val;
    bool first = true;
    IntType ibyteTy("", bits_byte);

    for (auto &b: bytes) {
      StateValue v(b.nonptrValue(),
                   ibyteTy.combine_poison(!b.isPtr(), b.nonptrNonpoison()));
      val = first ? std::move(v) : v.concat(val);
      first = false;
    }
    return toType.fromInt(val.trunc(bitsize, toType.np_bits()));
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
    Pointer p(m, ptr_val);
    auto offset = p.getOffset();
    for (auto &bid : p.getBid().leafs()) {
      ptrs.emplace(m, bid, offset);
    }
  }
  return ptrs;
}

static set<expr> extract_possible_local_bids(Memory &m, const expr &eptr) {
  set<expr> ret;
  for (auto &ptr : all_leaf_ptrs(m, eptr)) {
    if (!ptr.isLocal().isFalse())
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

unsigned Memory::numLocals() const {
  return state->isSource() ? num_locals_src : num_locals_tgt;
}

unsigned Memory::numNonlocals() const {
  return state->isSource() ? num_nonlocals_src : num_nonlocals;
}

expr Memory::isBlockAlive(const expr &bid, bool local) const {
  return
    load_bv(local ? local_block_liveness : non_local_block_liveness, bid) &&
      (!local && has_null_block && !null_is_dereferenceable ? bid != 0 : true);
}

bool Memory::mayalias(bool local, unsigned bid0, const expr &offset0,
                      unsigned bytes, uint64_t align, bool write) const {
  if (local && bid0 >= next_local_bid)
    return false;

  if (!local) {
    if (bid0 >= numNonlocals())         return false;
    if (!write && always_noread(bid0))  return false;
    if ( write && always_nowrite(bid0)) return false;
  }

  if (offset0.isNegative().isTrue())
    return false;

  assert(!isUndef(offset0));

  expr bid = expr::mkUInt(bid0, Pointer::bitsShortBid());
  if (auto algn = (local ? local_blk_align : non_local_blk_align).lookup(bid)) {
    int64_t offset = 0;
    uint64_t blk_align;
    if (algn->isUInt(blk_align) &&
        align > (1ull << blk_align) &&
        (!observesAddresses() || offset0.isInt(offset)))
      return false;
  }

  if (auto sz = (local ? local_blk_size : non_local_blk_size).lookup(bid)) {
    expr offset = offset0.sextOrTrunc(bits_size_t);
    if (offset.uge(*sz).isTrue() || (*sz - offset).ult(bytes).isTrue())
      return false;
  } else if (local) // allocated in another branch
    return false;

  if (local || !always_alive(bid0)) {
    if ((local ? local_block_liveness : non_local_block_liveness)
          .extract(bid0, bid0).isZero())
      return false;
  }

  return true;
}

Memory::AliasSet Memory::computeAliasing(const Pointer &ptr, unsigned bytes,
                                         uint64_t align, bool write) const {
  assert(bytes % (bits_byte/8) == 0);

  AliasSet aliasing(*this);
  auto sz_local = aliasing.size(true);
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
    expr offset = p.getOffset();
    uint64_t bid;
    if (shortbid.isUInt(bid)) {
      if (!is_local.isFalse() && bid < sz_local)
        check_alias(this_alias, true, bid, offset);
      if (!is_local.isTrue() && bid < sz_nonlocal)
        check_alias(this_alias, false, bid, offset);
      goto end;
    }

    for (auto local : { true, false }) {
      if ((local && is_local.isFalse()) || (!local && is_local.isTrue()))
        continue;
      for (unsigned i = 0, e = local ? sz_local : sz_nonlocal; i < e; ++i) {
        if (write && !local && always_nowrite(i))
          continue;
        if (!write && !local && always_noread(i))
          continue;
        check_alias(this_alias, local, i, offset);
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

template <typename Fn>
void Memory::access(const Pointer &ptr, unsigned bytes, uint64_t align,
                    bool write, Fn &fn) {
  auto aliasing = computeAliasing(ptr, bytes, align, write);
  unsigned has_local = aliasing.numMayAlias(true);
  unsigned has_nonlocal = aliasing.numMayAlias(false);
  bool has_both = has_local && has_nonlocal;
  bool is_singleton = has_local + has_nonlocal == 1;

  expr is_local = ptr.isLocal();
  expr bid = has_both ? ptr.getBid() : ptr.getShortBid();
  expr one = expr::mkUInt(1, 1);

  auto sz_local = aliasing.size(true);
  auto sz_nonlocal = aliasing.size(false);

  for (unsigned i = 0; i < sz_local; ++i) {
    if (aliasing.mayAlias(true, i)) {
      auto n = expr::mkUInt(i, Pointer::bitsShortBid());
      fn(local_block_val[i], i, true,
         is_singleton ? true
                      : (has_local == 1
                           ? is_local
                           : bid == (has_both ? one.concat(n) : n)));
    }
  }

  for (unsigned i = 0; i < sz_nonlocal; ++i) {
    if (aliasing.mayAlias(false, i)) {
      // A nonlocal block for encoding fn calls' side effects cannot be
      // accessed.
      // If aliasing info says it can, either imprecise analysis or incorrect
      // block id encoding is happening.
      assert(!is_fncall_mem(i));
      fn(non_local_block_val[i], i, false,
         is_singleton ? true : (has_nonlocal == 1 ? !is_local : bid == i));
    }
  }
}

vector<Byte> Memory::load(const Pointer &ptr, unsigned bytes, set<expr> &undef,
                          uint64_t align, bool left2right, DataType type) {
  if (bytes == 0)
    return {};

  unsigned bytesz = (bits_byte / 8);
  unsigned loaded_bytes = bytes / bytesz;
  vector<DisjointExpr<expr>> loaded;
  loaded.resize(loaded_bytes, Byte::mkPoisonByte(*this)());

  expr offset = ptr.getShortOffset();
  unsigned off_bits = Pointer::bitsShortOffset();

  auto fn = [&](const MemBlock &blk, unsigned bid, bool local, expr &&cond) {
    bool is_poison = (type & blk.type) == DATA_NONE;
    for (unsigned i = 0; i < loaded_bytes; ++i) {
      unsigned idx = left2right ? i : (loaded_bytes - i - 1);
      expr off = offset + expr::mkUInt(idx, off_bits);
      loaded[i].add(is_poison ? Byte::mkPoisonByte(*this)()
                              : blk.val.load(off), cond);
      if (!is_poison)
        undef.insert(blk.undef.begin(), blk.undef.end());
    }
  };

  access(ptr, bytes, align, false, fn);

  vector<Byte> ret;
  for (auto &disj : loaded) {
    ret.emplace_back(*this, *std::move(disj)());
  }
  return ret;
}

Memory::DataType Memory::data_type(const vector<pair<unsigned, expr>> &data,
                                   bool full_store) const {
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
  expr offset = ptr.getShortOffset();
  unsigned off_bits = Pointer::bitsShortOffset();

  auto stored_ty = data_type(data, false);
  auto stored_ty_full = data_type(data, true);

  auto fn = [&](MemBlock &blk, unsigned bid, bool local, expr &&cond) {
    auto mem = blk.val;

    uint64_t blk_size;
    bool full_write = false;
    // optimization: if fully rewriting the block, don't bother with the old
    // contents. Pick a value as the default one.
    if (Pointer(*this, bid, local).blockSize().isUInt(blk_size) &&
        blk_size == bytes) {
      mem = expr::mkConstArray(offset, data[0].second);
      full_write = true;
      if (cond.isTrue()) {
        blk.undef.clear();
        blk.type = stored_ty_full;
      } else {
        blk.type |= stored_ty_full;
      }
    } else {
      blk.type |= stored_ty;
    }

    for (auto &[idx, val] : data) {
      if (full_write && val.eq(data[0].second))
        continue;
      expr off
       = offset + expr::mkUInt(idx >> Pointer::zeroBitsShortOffset(), off_bits);
      mem = mem.store(off, val);
    }
    blk.val = expr::mkIf(cond, mem, blk.val);
    blk.undef.insert(undef.begin(), undef.end());
  };

  access(ptr, bytes, align, !state->isInitializationPhase(), fn);
}

void Memory::storeLambda(const Pointer &ptr, const expr &offset,
                         const expr &bytes,
                         const vector<pair<unsigned, expr>> &data,
                         const set<expr> &undef, uint64_t align) {
  assert(!state->isInitializationPhase());
  // offset in [ptr, ptr+sz)
  auto offset_cond = offset.uge(ptr.getShortOffset()) &&
                     offset.ult((ptr + bytes).getShortOffset());

  bool val_no_offset = data.size() == 1 && !data[0].second.vars().count(offset);
  auto stored_ty = data_type(data, false);

  expr val = data.back().second;
  expr mod = expr::mkUInt(data.size(), offset);
  for (auto I = next(data.rbegin()), E = data.rend(); I != E; ++I) {
    val = expr::mkIf(offset.urem(mod) == I->first, I->second, val);
  }

  auto fn = [&](MemBlock &blk, unsigned bid, bool local, expr &&cond) {
    // optimization: full rewrite
    if (bytes.eq(Pointer(*this, bid, local).blockSize())) {
      blk.val = val_no_offset && data.size() == 1
        ? expr::mkIf(cond, expr::mkConstArray(offset, val), blk.val)
        : expr::mkLambda(offset, expr::mkIf(cond, val, blk.val.load(offset)));

      if (cond.isTrue()) {
        blk.undef.clear();
        blk.type = stored_ty;
      }
    } else {
      blk.val = expr::mkLambda(offset, expr::mkIf(cond && offset_cond, val,
                                                  blk.val.load(offset)));
    }
    blk.type |= stored_ty;
    blk.undef.insert(undef.begin(), undef.end());
  };

  uint64_t size = bits_byte / 8;
  bytes.isUInt(size);

  access(ptr, size, align, true, fn);
}

static bool memory_unused() {
  return num_locals_src == 0 && num_locals_tgt == 0 && num_nonlocals == 0;
}

static expr mk_block_val_array(unsigned bid) {
  auto str = "init_mem_" + to_string(bid);
  return expr::mkArray(str.c_str(),
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

void Memory::mkNonlocalValAxioms(bool skip_consts) {
  expr offset
    = expr::mkFreshVar("#off", expr::mkUInt(0, Pointer::bitsShortOffset()));

  // Users may request the initial memory to be non-poisonous
  if (config::disable_poison_input && state->isSource() &&
      (does_int_mem_access || does_ptr_mem_access)) {
    for (auto &block : non_local_block_val) {
      if (isInitialMemBlock(block.val))
        state->addAxiom(
          expr::mkForAll({ offset },
                         !Byte(*this, block.val.load(offset)).isPoison()));
    }
  }

  if (!does_ptr_mem_access)
    return;

  for (unsigned i = 0, e = numNonlocals(); i != e; ++i) {
    if (always_noread(i, true))
      continue;

    Byte byte(*this, non_local_block_val[i].val.load(offset));
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
      expr::mkForAll({ offset },
        byte.isPtr().implies(!loadedptr.isLocal(false) &&
                             !loadedptr.isNocapture(false) &&
                             std::move(bid_cond))));
  }
}

Memory::Memory(State &state) : state(&state), escaped_local_blks(*this) {
  if (memory_unused())
    return;

  next_nonlocal_bid
    = has_null_block + num_globals_src + num_ptrinputs + has_fncall;

  bool skip_null = has_null_block && !null_is_dereferenceable;
  if (skip_null)
    non_local_block_val.emplace_back();

  // TODO: should skip initialization of fully initialized constants
  for (unsigned bid = skip_null, e = numNonlocals(); bid != e; ++bid) {
    non_local_block_val.emplace_back(mk_block_val_array(bid));
  }

  non_local_block_liveness = mk_liveness_array();

  // Non-local blocks cannot initially contain pointers to local blocks
  // and no-capture pointers.
  mkNonlocalValAxioms(false);

  // initialize all local blocks as non-pointer, poison value
  // This is okay because loading a pointer as non-pointer is also poison.
  if (numLocals() > 0) {
    auto poison_array
      = expr::mkConstArray(expr::mkUInt(0, Pointer::bitsShortOffset()),
                           Byte::mkPoisonByte(*this)());
    local_block_val.resize(numLocals(), { std::move(poison_array), DATA_NONE });

    // all local blocks are dead in the beginning
    local_block_liveness = expr::mkUInt(0, numLocals());
  }

  // Initialize a memory block for null pointer.
  if (skip_null)
    alloc(expr::mkUInt(0, bits_size_t), bits_byte / 8, GLOBAL, false, false, 0);
}

void Memory::mkAxioms(const Memory &tgt) const {
  assert(state->isSource() && !tgt.state->isSource());
  if (memory_unused())
    return;

  auto skip_bid = [&](unsigned bid) {
    if (bid == 0 && has_null_block)
      return true;
    if (is_globalvar(bid, true))
      return false;
    if (is_fncall_mem(bid))
      return true;
    return bid >= tgt.next_nonlocal_bid;
  };

  // transformation can increase alignment
  expr align = expr::mkUInt(ilog2(heap_block_alignment), 6);

  if (null_is_dereferenceable && has_null_block) {
    state->addAxiom(Pointer::mkNullPointer(*this).blockAlignment() == -1u);
    state->addAxiom(Pointer::mkNullPointer(tgt).blockAlignment() == -1u);
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
      state->addAxiom(p_align.ule(q_align));
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

  // Non-local blocks are disjoint.
  for (unsigned bid = 0; bid < num_nonlocals; ++bid) {
    if (skip_bid(bid))
      continue;

    Pointer p1(*this, bid, false);
    auto addr  = p1.getAddress();
    auto sz    = p1.blockSize().zextOrTrunc(bits_ptr_address);
    auto align = p1.blockAlignment();

    if (!has_null_block || bid != 0)
      state->addAxiom(addr != 0);

    state->addAxiom(
      Pointer::hasLocalBit()
        // don't spill to local addr section
        ? (addr + sz).sign() == 0
        : addr.add_no_uoverflow(sz));

    // disjointness constraint
    for (unsigned bid2 = bid + 1; bid2 < num_nonlocals; ++bid2) {
      if (skip_bid(bid2))
        continue;
      Pointer p2(*this, bid2, false);
      state->addAxiom(disjoint(addr, sz, align, p2.getAddress(),
                               p2.blockSize().zextOrTrunc(bits_ptr_address),
                               p2.blockAlignment()));
    }
  }
}

void Memory::resetGlobals() {
  Pointer::resetGlobals();
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
  // TODO: copy alias info for fn return ptrs from src?
}

void Memory::markByVal(unsigned bid, bool is_const) {
  assert(is_globalvar(bid, false));
  byval_blks.emplace_back(bid, is_const);
}

expr Memory::mkInput(const char *name, const ParamAttrs &attrs) {
  unsigned max_bid = has_null_block + num_globals_src + next_ptr_input++;
  assert(max_bid < num_nonlocals_src);
  Pointer p(*this, name, false, false, false, attrs);
  auto bid = p.getShortBid();

  state->addAxiom(bid.ule(max_bid));

  AliasSet alias(*this);
  alias.setMayAliasUpTo(false, max_bid);

  for (auto [byval_bid, is_const] : byval_blks) {
    state->addAxiom(bid != byval_bid);
    alias.setNoAlias(false, byval_bid);
  }
  ptr_alias.emplace(p.getBid(), std::move(alias));

  return std::move(p).release();
}

pair<expr, expr> Memory::mkUndefInput(const ParamAttrs &attrs) const {
  bool nonnull = attrs.has(ParamAttrs::NonNull);
  unsigned log_offset = ilog2_ceil(bits_for_offset, false);
  unsigned bits_undef = bits_for_offset + nonnull * log_offset;
  expr undef = expr::mkFreshVar("undef", expr::mkUInt(0, bits_undef));
  expr offset = undef;

  if (nonnull) {
    expr var = undef.extract(log_offset - 1, 0);
    expr one = expr::mkUInt(1, bits_for_offset);
    expr shl = expr::mkIf(var.ugt(bits_for_offset-1),
                          one,
                          one << var.zextOrTrunc(bits_for_offset));
    offset = undef.extract(bits_undef - 1, log_offset) | shl;
  }
  return
    { Pointer(*this, expr::mkUInt(0, bits_for_bid), offset, attrs).release(),
      std::move(undef) };
}

bool Memory::PtrInput::eq_attrs(const PtrInput &rhs) const {
  return byval == rhs.byval &&
         noread == rhs.noread &&
         nowrite == rhs.nowrite &&
         nocapture == rhs.nocapture;
}

expr Memory::PtrInput::operator==(const PtrInput &rhs) const {
  if (!eq_attrs(rhs))
    return false;
  return val == rhs.val;
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
                                         expr::mkUInt(0, 6));

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

Memory::CallState Memory::CallState::mkIf(const expr &cond,
                                          const CallState &then,
                                          const CallState &els) {
  CallState ret;
  for (unsigned i = 0, e = then.non_local_block_val.size(); i != e; ++i) {
    ret.non_local_block_val.emplace_back(
      expr::mkIf(cond, then.non_local_block_val[i],
                 els.non_local_block_val[i]));
  }
  ret.non_local_liveness = expr::mkIf(cond, then.non_local_liveness,
                                      els.non_local_liveness);
  return ret;
}

expr Memory::CallState::operator==(const CallState &rhs) const {
  if (empty != rhs.empty)
    return false;
  if (empty)
    return true;

  expr ret = non_local_liveness == rhs.non_local_liveness;
  for (unsigned i = 0, e = non_local_block_val.size(); i != e; ++i) {
    ret &= non_local_block_val[i] == rhs.non_local_block_val[i];
  }
  return ret;
}

Memory::CallState
Memory::mkCallState(const string &fnname, bool nofree, MemoryAccess access) {
  assert(has_fncall);
  CallState st;
  st.empty = false;

  // TODO: handle havoc of local blocks

  auto blk_type = mk_block_val_array(1);

  if (access.canOnlyWrite(MemoryAccess::Inaccessible)) {
    st.non_local_block_val.emplace_back(expr::mkFreshVar("blk_val", blk_type));
  } else {
    unsigned limit = num_nonlocals_src - num_inaccessiblememonly_fns;
    for (unsigned i = 0; i < limit; ++i) {
      if (always_nowrite(i, true, true))
        continue;
      st.non_local_block_val.emplace_back(
        expr::mkFreshVar("blk_val", blk_type));
    }
  }

  st.non_local_liveness = mk_liveness_array();
  if (num_nonlocals_src && !nofree &&
      !access.canOnlyWrite(MemoryAccess::Inaccessible))
    st.non_local_liveness
      = expr::mkFreshVar("blk_liveness", st.non_local_liveness);

  return st;
}

void Memory::setState(const Memory::CallState &st,
                      MemoryAccess access,
                      const vector<PtrInput> &ptr_inputs,
                      unsigned inaccessible_bid) {
  assert(has_fncall);

  unsigned limit = num_nonlocals_src - num_inaccessiblememonly_fns;

  // 1) Havoc memory

  if (access.canOnlyWrite(MemoryAccess::Inaccessible)) {
    assert(inaccessible_bid != -1u);
    assert(st.non_local_block_val.size() == 1);
    unsigned bid
      = num_nonlocals_src - num_inaccessiblememonly_fns + inaccessible_bid;
    assert(bid < num_nonlocals_src);
    assert(non_local_block_val[bid].undef.empty());
    non_local_block_val[bid].val = st.non_local_block_val[0];
  }

  if (access.canWrite(MemoryAccess::Args) &&
     !access.canOnlyWrite(MemoryAccess::Other)) {
    unsigned idx = 0;
    for (unsigned bid = 0; bid < limit - has_write_fncall; ++bid) {
      if (always_nowrite(bid, true, true))
        continue;

      expr modifies(false);
      for (auto &ptr_in : ptr_inputs) {
        if (ptr_in.nowrite)
          continue;

        if (!ptr_in.byval && bid < next_nonlocal_bid) {
          modifies |= ptr_in.val.non_poison &&
                      Pointer(*this, ptr_in.val.value).getBid() == bid;
        }
      }

      auto &new_val = st.non_local_block_val[idx++];
      auto &cur_val = non_local_block_val[bid].val;
      cur_val = expr::mkIf(modifies, new_val, cur_val);
      if (modifies.isTrue())
        non_local_block_val[bid].undef.clear();
    }
    assert(idx == st.non_local_block_val.size() - has_write_fncall);
  }

  if (access.canWrite(MemoryAccess::Errno)) {
    // TODO
   }

  if (access.canWrite(MemoryAccess::Other)) {
    unsigned idx = 0;
    for (unsigned bid = 0; bid < limit; ++bid) {
      if (always_nowrite(bid, true, true))
        continue;
      non_local_block_val[bid].val = st.non_local_block_val[idx++];
      non_local_block_val[bid].undef.clear();
    }
    assert(idx == st.non_local_block_val.size());
  }

  if (!st.non_local_liveness.isAllOnes()) {
    expr one  = expr::mkUInt(1, num_nonlocals);
    expr zero = expr::mkUInt(0, num_nonlocals);
    expr mask = always_nowrite(0) ? one : zero;
    for (unsigned bid = always_nowrite(0); bid < num_nonlocals; ++bid) {
      expr may_free = true;
      if (!access.canWrite(MemoryAccess::Other)) {
        may_free = false;
        for (auto &ptr_in : ptr_inputs) {
          if (!ptr_in.byval && bid < next_nonlocal_bid)
            may_free |= ptr_in.val.non_poison &&
                        Pointer(*this, ptr_in.val.value).getBid() == bid;
        }
      }
      expr heap = Pointer(*this, bid, false).isHeapAllocated();
      mask = mask | expr::mkIf(heap && may_free && !is_fncall_mem(bid),
                               zero,
                               one << expr::mkUInt(bid, one));
    }

    // functions can free an object, but cannot bring a dead one back to live
    non_local_block_liveness
      = non_local_block_liveness & (st.non_local_liveness | mask);
  }

  mkNonlocalValAxioms(true);
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
                                p2.blockSize().zextOrTrunc(bits_ptr_address),
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
Memory::alloc(const expr &size, uint64_t align, BlockKind blockKind,
              const expr &precond, const expr &nonnull,
              optional<unsigned> bidopt, unsigned *bid_out) {
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

  expr size_zext = size.zextOrTrunc(bits_size_t);
  expr nooverflow = size.bits() <= bits_size_t ? true :
                      size.extract(size.bits()-1, bits_size_t) == 0;

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
  expr align_expr = expr::mkUInt(align_bits, 6);
  bool is_null = !is_local && has_null_block && bid == 0;

  if (is_local) {
    mkLocalDisjAddrAxioms(allocated, short_bid, size_zext, align_expr,
                          align_bits);
  } else {
    state->addAxiom(p.blockSize() == size_zext);
    state->addAxiom(p.isBlockAligned(align, true));
    state->addAxiom(p.getAllocType() == alloc_ty);

    if (align_bits && observesAddresses())
      state->addAxiom(p.getAddress().extract(align_bits - 1, 0) == 0);

    assert(is_const == is_constglb(bid, state->isSource()));
    assert((has_null_block && bid == 0) ||
           is_globalvar(bid, !state->isSource()));
  }

  if (!is_null)
    store_bv(p, allocated, local_block_liveness, non_local_block_liveness);
  (is_local ? local_blk_size : non_local_blk_size)
    .add(short_bid, std::move(size_zext));
  (is_local ? local_blk_align : non_local_blk_align)
    .add(short_bid, std::move(align_expr));
  (is_local ? local_blk_kind : non_local_blk_kind)
    .add(short_bid, expr::mkUInt(alloc_ty, 2));

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
                            p.blockSize().zextOrTrunc(bits_ptr_address),
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
    vector<Byte> bytes = valueToBytes(v, type, *this, state);
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
    state->addUB(ptr.isDereferenceable(bytesize, align, true));

  auto wval = val;
  for (unsigned i = 1; i < bytesz; ++i) {
    wval = wval.concat(val);
  }
  assert(!val.isValid() || wval.bits() == bits_byte);

  auto bytes = valueToBytes(wval, IntType("", bits_byte), *this, state);
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
    expr offset
      = expr::mkFreshVar("#off", expr::mkUInt(0, Pointer::bitsShortOffset()));
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
    expr offset
      = expr::mkFreshVar("#off", expr::mkUInt(0, Pointer::bitsShortOffset()));
    storeLambda(ptr, offset, bytesize, to_store, undef_vars, 1);
  }
}

void Memory::memcpy(const expr &d, const expr &s, const expr &bytesize,
                    uint64_t align_dst, uint64_t align_src, bool is_move) {
  assert(!memory_unused());
  unsigned bytesz = bits_byte / 8;

  Pointer dst(*this, d), src(*this, s);
  state->addUB(dst.isDereferenceable(bytesize, align_dst, true));
  state->addUB(src.isDereferenceable(bytesize, align_src, false));
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
    expr offset
      = expr::mkFreshVar("#off", expr::mkUInt(0, Pointer::bitsShortOffset()));
    Pointer ptr_src = src + (offset - dst.getShortOffset());
    set<expr> undef;
    auto val = raw_load(ptr_src, undef);
    storeLambda(dst, offset, bytesize, {{0, std::move(val)()}}, undef,
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
  ENSURE(dst.getShortBid().isUInt(dst_bid));
  auto &dst_blk = (dst_local ? local_block_val : non_local_block_val)[dst_bid];
  dst_blk.undef.clear();
  dst_blk.type = DATA_NONE;

  auto offset = expr::mkUInt(0, Pointer::bitsShortOffset());
  DisjointExpr val(expr::mkConstArray(offset, Byte::mkPoisonByte(*this)()));

  auto fn = [&](MemBlock &blk, unsigned bid, bool local, expr &&cond) {
    // we assume src != dst
    if (local == dst_local && bid == dst_bid)
      return;
    val.add(blk.val, std::move(cond));
    dst_blk.undef.insert(blk.undef.begin(), blk.undef.end());
    dst_blk.type |= blk.type;
  };
  access(src, bits_byte/8, bits_byte/8, false, fn);
  dst_blk.val = *std::move(val)();
}

void Memory::fillPoison(const expr &bid) {
  Pointer p(*this, bid, expr::mkUInt(0, bits_for_offset));
  expr blksz = p.blockSize();
  memset(std::move(p).release(), IntType("i8", 8).getDummyValue(false),
         std::move(blksz), bits_byte / 8, {}, false);
}

expr Memory::ptr2int(const expr &ptr) const {
  assert(!memory_unused() && observesAddresses());
  Pointer p(*this, ptr);
  state->addUB(!p.isNocapture());
  return p.getAddress();
}

expr Memory::int2ptr(const expr &val) const {
  assert(!memory_unused() && observesAddresses());
  // TODO: missing pointer escaping
  if (config::enable_approx_int2ptr) {
    DisjointExpr<expr> ret(expr{});
    expr valx = val.zextOrTrunc(bits_program_pointer);

    auto add = [&](unsigned limit, bool local) {
      for (unsigned i = 0; i != limit; ++i) {
        Pointer p(*this, i, local);
        Pointer p_end = p + p.blockSize();
        ret.add((p + (valx - p.getAddress())).release(),
                valx.uge(p.getAddress()) && valx.ule(p_end.getAddress()));
      }
    };
    add(numLocals(), true);
    add(numNonlocals(), false);
    return *std::move(ret)();
  }

  expr null = Pointer::mkNullPointer(*this).release();
  expr fn = expr::mkUF("int2ptr", { val }, null);
  state->doesApproximation("inttoptr", fn);
  return expr::mkIf(val == 0, null, fn);
}

expr Memory::blockValRefined(const Memory &other, unsigned bid, bool local,
                             const expr &offset, set<expr> &undef) const {
  assert(!local);
  auto &mem1 = non_local_block_val[bid];
  auto &mem2 = other.non_local_block_val[bid].val;

  if (mem1.val.eq(mem2))
    return true;

  int is_fn1 = isInitialMemBlock(mem1.val, true);
  int is_fn2 = isInitialMemBlock(mem2, true);
  if (is_fn1 && is_fn2) {
    // if both memories are the result of a function call, then refinement
    // holds iff they are equal, otherwise we can always force a behavior
    // of the function such that it will store a different value to memory
    if (is_fn1 == 2 && is_fn2 == 2)
      return mem1.val == mem2;

    // an inital memory (m0) vs a function call is always false, as a function
    // may always store something to memory
    assert((is_fn1 == 1 && is_fn2 == 2) || (is_fn1 == 2 && is_fn2 == 1));
    return false;
  }

  Byte val(*this, mem1.val.load(offset));
  Byte val2(other, mem2.load(offset));

  if (val.eq(val2))
    return true;

  undef.insert(mem1.undef.begin(), mem1.undef.end());

  return val.refined(val2);
}

expr Memory::blockRefined(const Pointer &src, const Pointer &tgt, unsigned bid,
                          set<expr> &undef) const {
  unsigned bytes_per_byte = bits_byte / 8;

  expr blk_size = src.blockSize();
  expr ptr_offset = src.getShortOffset();
  expr val_refines;

  uint64_t bytes;
  if (blk_size.isUInt(bytes) && (bytes / bytes_per_byte) <= 8) {
    val_refines = true;
    for (unsigned off = 0; off < (bytes / bytes_per_byte); ++off) {
      expr off_expr = expr::mkUInt(off, Pointer::bitsShortOffset());
      val_refines
        &= (ptr_offset == off_expr).implies(
             blockValRefined(tgt.getMemory(), bid, false, off_expr, undef));
    }
  } else {
    val_refines
      = src.getOffsetSizet().ult(src.blockSizeOffsetT()).implies(
          blockValRefined(tgt.getMemory(), bid, false, ptr_offset, undef));
  }

  expr aligned(true);
  expr src_align = src.blockAlignment();
  expr tgt_align = tgt.blockAlignment();
  // if they are both non-const, then the condition holds per the precondition
  if (src_align.isConst() || tgt_align.isConst())
    aligned = src_align.ule(tgt_align);

  expr alive = src.isBlockAlive();
  return alive == tgt.isBlockAlive() &&
         blk_size == tgt.blockSize() &&
         src.getAllocType() == tgt.getAllocType() &&
         aligned &&
         alive.implies(val_refines);
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
  for (auto &[mem, set]
         : { make_pair(this, set_ptrs), make_pair(&other, set_ptrs2)}) {
    if (set) {
      for (auto &it: *set_ptrs) {
        block_alias.unionWith(computeAliasing(Pointer(*mem, it.val.value),
                                              min_read_sz, min_read_sz, false));
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

    expr bid_expr = expr::mkUInt(bid, bits_for_bid);
    Pointer p(*this, bid_expr, offset);
    Pointer q(other, p());
    if (p.isByval().isTrue() && q.isByval().isTrue())
      continue;
    ret &= (ptr_bid == bid_expr).implies(blockRefined(p, q, bid, undef_vars));
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
    Byte b(*this, non_local_block_val[bid].val.load(offset));
    Pointer loadp(*this, b.ptrValue());
    res &= (p.isBlockAlive() && b.isPtr() && b.ptrNonpoison())
             .implies(!loadp.isNocapture());
  }
  if (!res.isTrue())
    state->addQuantVar(offset);
  return res;
}

void Memory::escapeLocalPtr(const expr &ptr, const expr &is_ptr) {
  if (is_ptr.isFalse())
    return;

  if (next_local_bid == 0 ||
      escaped_local_blks.isFullUpToAlias(true) == (int)next_local_bid-1)
    return;

  uint64_t bid;
  for (const auto &bid_expr : extract_possible_local_bids(*this, ptr)) {
    if (bid_expr.isUInt(bid)) {
      if (bid < next_local_bid)
        escaped_local_blks.setMayAlias(true, bid);
    } else if (isInitialMemBlock(bid_expr)) {
      // initial non local block bytes don't contain local pointers.
    } else if (isFnReturnValue(bid_expr)) {
      // Function calls have already escaped whatever they needed to.
    } else {
      // may escape a local ptr, but we don't know which one
      escaped_local_blks.setMayAliasUpTo(true, next_local_bid-1);
      break;
    }
  }
}

Memory Memory::mkIf(const expr &cond, Memory &&then, Memory &&els) {
  assert(then.state == els.state);
  Memory &ret = then;
  for (unsigned bid = 0, end = ret.numNonlocals(); bid < end; ++bid) {
    if (always_nowrite(bid, false, true))
      continue;
    auto &other = els.non_local_block_val[bid];
    ret.non_local_block_val[bid].val
      = expr::mkIf(cond, then.non_local_block_val[bid].val, other.val);
    ret.non_local_block_val[bid].undef.insert(other.undef.begin(),
                                              other.undef.end());
  }
  for (unsigned bid = 0, end = ret.numLocals(); bid < end; ++bid) {
    auto &other = els.local_block_val[bid];
    ret.local_block_val[bid].val
      = expr::mkIf(cond, then.local_block_val[bid].val, other.val);
    ret.local_block_val[bid].undef.insert(other.undef.begin(),
                                          other.undef.end());
  }
  ret.non_local_block_liveness = expr::mkIf(cond, then.non_local_block_liveness,
                                            els.non_local_block_liveness);
  ret.local_block_liveness     = expr::mkIf(cond, then.local_block_liveness,
                                            els.local_block_liveness);
  ret.local_blk_addr.add(els.local_blk_addr);
  ret.local_blk_size.add(els.local_blk_size);
  ret.local_blk_align.add(els.local_blk_align);
  ret.local_blk_kind.add(els.local_blk_kind);
  ret.non_local_blk_size.add(els.non_local_blk_size);
  ret.non_local_blk_align.add(els.non_local_blk_align);
  ret.non_local_blk_kind.add(els.non_local_blk_kind);
  ret.escaped_local_blks.unionWith(els.escaped_local_blks);

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
      if (observesAddresses())
        P("address", p.getAddress());
      os << '\n';
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
  if (!m.ptr_alias.empty()) {
    os << "\nALIAS SETS:\n";
    for (auto &[bid, alias] : m.ptr_alias) {
      os << bid << ": ";
      alias.print(os);
      os << '\n';
    }
  }
  return os;
}

}
