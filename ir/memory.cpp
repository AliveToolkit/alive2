// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/memory.h"
#include "ir/globals.h"
#include "ir/state.h"
#include "ir/value.h"
#include "smt/solver.h"
#include "util/compiler.h"
#include <string>

using namespace IR;
using namespace smt;
using namespace std;
using namespace util;

#define mkIf_fold(c, a, b) \
  mkIf_fold_fn(c, [&]() { return a; }, [&]() { return b; })

template <typename T1, typename T2>
static expr mkIf_fold_fn(const expr &cond, T1 &&a, T2 &&b) {
  if (cond.isTrue())
    return a();
  if (cond.isFalse())
    return b();
  return expr::mkIf(cond, a(), b());
}

static unsigned ptr_next_idx = 0;

static unsigned zero_bits_offset() {
  assert(is_power2(bits_byte));
  return ilog2(bits_byte / 8);
}

static bool byte_has_ptr_bit() {
  return does_int_mem_access && does_ptr_mem_access;
}

static unsigned bits_int_poison() {
  return does_sub_byte_access ? bits_byte : 1;
}

static unsigned bits_ptr_byte_offset() {
  assert(!does_ptr_mem_access || bits_byte <= bits_program_pointer);
  return bits_byte < bits_program_pointer ? 3 : 0;
}

static unsigned padding_ptr_byte() {
  return Byte::bitsByte() - does_int_mem_access - 1 - Pointer::totalBits()
                          - bits_ptr_byte_offset();
}

static unsigned padding_nonptr_byte() {
  return Byte::bitsByte() - does_ptr_mem_access - bits_byte - bits_int_poison();
}

static expr concat_if(const expr &ifvalid, expr &&e) {
  return ifvalid.isValid() ? ifvalid.concat(e) : move(e);
}

static expr prepend_if(const expr &pre, expr &&e, bool prepend) {
  return prepend ? pre.concat(e) : move(e);
}

static string local_name(const State *s, const char *name) {
  return string(name) + (s->isSource() ? "_src" : "_tgt");
}

static expr load_bv(const expr &var, const expr &idx0) {
  auto bw = var.bits();
  if (!bw)
    return {};
  auto idx = idx0.zextOrTrunc(bw);
  return var.lshr(idx).extract(0, 0) == 1;
}

static void store_bv(Pointer &p, const expr &val, expr &local,
                     expr &non_local, bool assume_local = false) {
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
  local = expr::mkIf(is_local, set(local), local);
  non_local = expr::mkIf(!is_local, set(non_local), non_local);
}

namespace IR {

Byte::Byte(const Memory &m, expr &&byterepr) : m(m), p(move(byterepr)) {
  assert(!p.isValid() || p.bits() == bitsByte());
}

Byte::Byte(const Pointer &ptr, unsigned i, const expr &non_poison)
  : m(ptr.getMemory()) {
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
                expr::mkIf(non_poison, expr::mkUInt(0, 1), expr::mkUInt(1, 1)))
      .concat(ptr());
  if (bits_ptr_byte_offset())
    p = p.concat(expr::mkUInt(i, bits_ptr_byte_offset()));
  p = p.concat_zeros(padding_ptr_byte());
  assert(!p.isValid() || p.bits() == bitsByte());
}

Byte::Byte(const Memory &m, const expr &data, const expr &non_poison) : m(m) {
  assert(!data.isValid() || data.bits() == bits_byte);
  assert(!non_poison.isValid() || non_poison.bits() == bits_int_poison());

  if (!does_int_mem_access) {
    p = expr::mkUInt(0, bitsByte());
    return;
  }

  if (byte_has_ptr_bit())
    p = expr::mkUInt(0, 1);
  p = concat_if(p, non_poison.concat(data).concat_zeros(padding_nonptr_byte()));
  assert(!p.isValid() || p.bits() == bitsByte());
}

expr Byte::isPtr() const {
  if (!does_ptr_mem_access)
    return false;
  if (!does_int_mem_access)
    return true;
  auto bit = p.bits() - 1;
  return p.extract(bit, bit) == 1;
}

expr Byte::ptrNonpoison() const {
  if (!does_ptr_mem_access)
    return true;
  auto bit = p.bits() - 1 - byte_has_ptr_bit();
  return p.extract(bit, bit) == 0;
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
    return expr::mkUInt(0, bits_int_poison());
  unsigned start = padding_nonptr_byte() + bits_byte;
  return p.extract(start + bits_int_poison() - 1, start);
}

expr Byte::nonptrValue() const {
  if (!does_int_mem_access)
    return expr::mkUInt(0, bits_byte);
  unsigned start = padding_nonptr_byte();
  return p.extract(start + bits_byte - 1, start);
}

expr Byte::isPoison(bool fullbit) const {
  expr np = nonptrNonpoison();
  if (byte_has_ptr_bit() && bits_int_poison() == 1) {
    assert(!np.isValid() || ptrNonpoison().eq(np == 0));
    return np == 1;
  }
  return expr::mkIf(isPtr(), !ptrNonpoison(),
                              fullbit ? np == -1ull : np != 0);
}

expr Byte::isZero() const {
  return expr::mkIf(isPtr(), ptr().isNull(), nonptrValue() == 0);
}

unsigned Byte::bitsByte() {
  unsigned ptr_bits = does_ptr_mem_access *
                        (1 + Pointer::totalBits() + bits_ptr_byte_offset());
  unsigned int_bits = does_int_mem_access * (bits_byte + bits_int_poison());
  // allow at least 1 bit if there's no memory access
  return max(1u, byte_has_ptr_bit() + max(ptr_bits, int_bits));
}

Byte Byte::mkPtrByte(const Memory &m, const expr &val) {
  assert(does_ptr_mem_access);
  expr byte;
  if (byte_has_ptr_bit())
    byte = expr::mkUInt(1, 1);
  return { m, concat_if(byte, expr(val))
                .concat_zeros(bits_for_ptrattrs + padding_ptr_byte()) };
}

Byte Byte::mkNonPtrByte(const Memory &m, const expr &val) {
  if (!does_int_mem_access)
    return { m, expr::mkUInt(0, bitsByte()) };
  expr byte;
  if (byte_has_ptr_bit())
    byte = expr::mkUInt(0, 1);
  return { m, concat_if(byte, expr(val)).concat_zeros(padding_nonptr_byte()) };
}

Byte Byte::mkPoisonByte(const Memory &m) {
  IntType ty("", bits_byte);
  auto v = ty.toBV(ty.getDummyValue(false));
  return { m, v.value, v.non_poison };
}

ostream& operator<<(ostream &os, const Byte &byte) {
  if (byte.isPoison().isTrue())
    return os << "poison";

  if (byte.isPtr().isTrue()) {
    os << byte.ptr() << ", byte offset=";
    byte.ptrByteoffset().printSigned(os);
  } else {
    auto np = byte.nonptrNonpoison();
    auto val = byte.nonptrValue();
    if (np.isZero()) {
      val.printHexadecimal(os);
    } else {
      os << "#b";
      for (unsigned i = 0; i < bits_int_poison(); ++i) {
        unsigned idx = bits_int_poison() - i - 1;
        auto is_poison = (np.extract(idx, idx) == 1).isTrue();
        auto v = (val.extract(idx, idx) == 1).isTrue();
        os << (is_poison ? 'p' : (v ? '1' : '0'));
      }
    }
  }
  return os;
}
}

static vector<Byte> valueToBytes(const StateValue &val, const Type &fromType,
                                 const Memory &mem, State *s) {
  vector<Byte> bytes;
  if (fromType.isPtrType()) {
    Pointer p(mem, val.value);
    unsigned bytesize = bits_program_pointer / bits_byte;

    for (unsigned i = 0; i < bytesize; ++i)
      bytes.emplace_back(p, i, val.non_poison);
  } else {
    assert(!fromType.isAggregateType() || isNonPtrVector(fromType));
    StateValue bvval = fromType.toInt(*s, val);
    unsigned bitsize = bvval.bits();
    unsigned bytesize = divide_up(bitsize, bits_byte);

    bvval = bvval.zext(bytesize * bits_byte - bitsize);
    unsigned np_mul = does_sub_byte_access ? bits_byte : 1;

    for (unsigned i = 0; i < bytesize; ++i) {
      expr data = bvval.value.extract((i + 1) * bits_byte - 1, i * bits_byte);
      expr np   = bvval.non_poison.extract((i + 1) * np_mul - 1, i * np_mul);
      bytes.emplace_back(mem, data, np);
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
        is_ptr     = move(b_is_ptr);
      } else {
        non_poison &= is_ptr == b_is_ptr;
      }
      non_poison &=
        expr::mkIf(is_ptr,
                   b.ptrByteoffset() == i && ptr_value == loaded_ptr,
                   b.nonptrValue() == 0);
      non_poison &= !b.isPoison(false);
    }
    return { expr::mkIf(is_ptr, loaded_ptr, Pointer::mkNullPointer(m)()),
             move(non_poison) };

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
      val = first ? move(v) : v.concat(val);
      first = false;
    }
    return toType.fromInt(val.trunc(bitsize));
  }
}

static bool observes_addresses() {
  return IR::has_ptr2int || IR::has_int2ptr;
}

static bool ptr_has_local_bit() {
  return (num_locals_src || num_locals_tgt) && num_nonlocals;
}

static unsigned bits_shortbid() {
  return bits_for_bid - ptr_has_local_bit();
}

static expr attr_to_bitvec(const ParamAttrs &attrs) {
  if (!bits_for_ptrattrs)
    return expr();

  uint64_t bits = 0;
  auto idx = 0;
  auto to_bit = [&](bool b, const ParamAttrs::Attribute &a) -> uint64_t {
    return b ? ((attrs.has(a) ? 1 : 0) << idx++) : 0;
  };
  bits |= to_bit(has_nocapture, ParamAttrs::NoCapture);
  bits |= to_bit(has_readonly, ParamAttrs::ReadOnly);
  bits |= to_bit(has_readnone, ParamAttrs::ReadNone);
  return expr::mkUInt(bits, bits_for_ptrattrs);
}

namespace IR {

Pointer::Pointer(const Memory &m, const char *var_name, const expr &local,
                 bool unique_name, bool align, const expr &attr) : m(m) {
  string name = var_name;
  if (unique_name)
    name += '!' + to_string(ptr_next_idx++);

  unsigned bits = totalBitsShort() + !align * zero_bits_offset();
  p = prepend_if(local.toBVBool(),
                 expr::mkVar(name.c_str(), bits), ptr_has_local_bit());
  if (align)
    p = p.concat_zeros(zero_bits_offset());
  if (bits_for_ptrattrs)
    p = p.concat(attr.isValid() ? attr : expr::mkUInt(0, bits_for_ptrattrs));
  assert(!local.isValid() || p.bits() == totalBits());
}

Pointer::Pointer(const Memory &m, expr repr) : m(m), p(move(repr)) {
  assert(!p.isValid() || p.bits() == totalBits());
}

Pointer::Pointer(const Memory &m, unsigned bid, bool local)
  : m(m), p(
    prepend_if(expr::mkUInt(local, 1),
               expr::mkUInt(bid, bits_shortbid())
                 .concat_zeros(bits_for_offset + bits_for_ptrattrs),
               ptr_has_local_bit())) {
  assert((local && bid < m.numLocals()) || (!local && bid < num_nonlocals));
  assert(p.bits() == totalBits());
}

Pointer::Pointer(const Memory &m, const expr &bid, const expr &offset,
                 const expr &attr) : m(m), p(bid.concat(offset)) {
  if (bits_for_ptrattrs)
    p = p.concat(attr.isValid() ? attr : expr::mkUInt(0, bits_for_ptrattrs));
  assert(!bid.isValid() || !offset.isValid() || p.bits() == totalBits());
}

unsigned Pointer::totalBits() {
  return bits_for_ptrattrs + bits_for_bid + bits_for_offset;
}

unsigned Pointer::totalBitsShort() {
  return bits_shortbid() + bits_for_offset - zero_bits_offset();
}

expr Pointer::isLocal() const {
  if (m.numLocals() == 0)
    return false;
  if (m.numNonlocals() == 0)
    return true;
  auto bit = totalBits() - 1;
  return p.extract(bit, bit) == 1;
}

expr Pointer::getBid() const {
  return p.extract(totalBits() - 1, bits_for_offset + bits_for_ptrattrs);
}

expr Pointer::getShortBid() const {
  return p.extract(totalBits() - 1 - ptr_has_local_bit(),
                   bits_for_offset + bits_for_ptrattrs);
}

expr Pointer::getOffset() const {
  return p.extract(bits_for_offset + bits_for_ptrattrs - 1, bits_for_ptrattrs);
}

expr Pointer::getOffsetSizet() const {
  return getOffset().sextOrTrunc(bits_size_t);
}

expr Pointer::getAttrs() const {
  return bits_for_ptrattrs ? p.extract(bits_for_ptrattrs - 1, 0) : expr();
}

expr Pointer::getValue(const char *name, const FunctionExpr &local_fn,
                       const FunctionExpr &nonlocal_fn,
                       const expr &ret_type, bool src_name) const {
  if (!p.isValid())
    return {};

  auto bid = getShortBid();
  expr non_local;

  if (auto val = nonlocal_fn.lookup(bid))
    non_local = *val;
  else {
    string uf = src_name ? local_name(m.state, name) : name;
    non_local = expr::mkUF(uf.c_str(), { bid }, ret_type);
  }

  if (auto local = local_fn(bid))
    return expr::mkIf(isLocal(), *local, non_local);
  return non_local;
}

expr Pointer::getAddress(bool simplify) const {
  assert(observes_addresses());

  auto bid = getShortBid();
  auto zero = expr::mkUInt(0, bits_size_t - 1);
  // fast path for null ptrs
  auto non_local
    = simplify && bid.isZero() && has_null_block ?
          zero : expr::mkUF("blk_addr", { bid }, zero);
  // Non-local block area is the lower half
  non_local = expr::mkUInt(0, 1).concat(non_local);

  expr addr;
  if (auto local = m.local_blk_addr(bid))
    // Local block area is the upper half of the memory
    addr = expr::mkIf(isLocal(), expr::mkUInt(1, 1).concat(*local), non_local);
  else
    addr = move(non_local);

  return addr + getOffsetSizet();
}

expr Pointer::blockSize() const {
  // ASSUMPTION: programs can only allocate up to half of address space
  // so the first bit of size is always zero.
  // We need this assumption to support negative offsets.
  return expr::mkUInt(0, 1)
           .concat(getValue("blk_size", m.local_blk_size, m.non_local_blk_size,
                             expr::mkUInt(0, bits_size_t - 1)));
}

expr Pointer::shortPtr() const {
  auto off = zero_bits_offset();
  return p.extract(totalBits() - 1 - ptr_has_local_bit(),
                   bits_for_ptrattrs + off);
}

Pointer Pointer::operator+(const expr &bytes) const {
  return { m, getBid(), getOffset() + bytes.zextOrTrunc(bits_for_offset),
           getAttrs() };
}

Pointer Pointer::operator+(unsigned bytes) const {
  return *this + expr::mkUInt(bytes, bits_for_offset);
}

void Pointer::operator+=(const expr &bytes) {
  p = (*this + bytes).p;
}

expr Pointer::addNoOverflow(const expr &offset) const {
  return getOffset().add_no_soverflow(offset);
}

expr Pointer::operator==(const Pointer &rhs) const {
  return p.extract(totalBits() - 1, bits_for_ptrattrs) ==
         rhs.p.extract(totalBits() - 1, bits_for_ptrattrs);
}

expr Pointer::operator!=(const Pointer &rhs) const {
  return !operator==(rhs);
}

#define DEFINE_CMP(op)                                                      \
StateValue Pointer::op(const Pointer &rhs) const {                          \
  /* Note that attrs are not compared. */                                   \
  expr nondet = expr::mkFreshVar("nondet", true);                           \
  m.state->addQuantVar(nondet);                                             \
  return { expr::mkIf(getBid() == rhs.getBid(),                             \
                      getOffset().op(rhs.getOffset()), nondet), true };     \
}

DEFINE_CMP(sle)
DEFINE_CMP(slt)
DEFINE_CMP(sge)
DEFINE_CMP(sgt)
DEFINE_CMP(ule)
DEFINE_CMP(ult)
DEFINE_CMP(uge)
DEFINE_CMP(ugt)

static expr inbounds(const Pointer &p, bool strict) {
  if (isUndef(p.getOffset()))
    return false;

  // equivalent to offset >= 0 && offset <= block_size
  // because block_size u<= 0x7FFF..FF
  return strict ? p.getOffsetSizet().ult(p.blockSize()) :
                  p.getOffsetSizet().ule(p.blockSize());
}

expr Pointer::inbounds(bool simplify_ptr, bool strict) {
  if (!simplify_ptr)
    return ::inbounds(*this, strict);

  DisjointExpr<expr> ret(expr(false)), all_ptrs;
  for (auto &[ptr_expr, domain] : DisjointExpr<expr>(p, true, true)) {
    expr inb = ::inbounds(Pointer(m, ptr_expr), strict);
    if (!inb.isFalse())
      all_ptrs.add(ptr_expr, domain);
    ret.add(move(inb), domain);
  }

  // trim set of valid ptrs
  if (auto ptrs = all_ptrs())
    p = *ptrs;
  else
    p = expr::mkUInt(0, totalBits());

  return *ret();
}

expr Pointer::blockAlignment() const {
  return getValue("blk_align", m.local_blk_align, m.non_local_blk_align,
                   expr::mkUInt(0, 8), true);
}

expr Pointer::isBlockAligned(unsigned align, bool exact) const {
  assert(align >= bits_byte / 8);
  if (!exact && align == 1)
    return true;

  auto bits = ilog2(align);
  expr blk_align = blockAlignment();
  return exact ? blk_align == bits : blk_align.uge(bits);
}

expr Pointer::isAligned(unsigned align) const {
  assert(align >= bits_byte / 8);
  if (align == 1)
    return true;

  if (isUndef(getOffset()))
    return false;

  auto bits = min(ilog2(align), bits_for_offset);

  if (!observes_addresses())
    // This is stricter than checking getAddress(), but as addresses are not
    // observed, program shouldn't be able to distinguish this from checking
    // getAddress()
    return isBlockAligned(align, false) &&
           getOffset().extract(bits - 1, 0) == 0;

  return getAddress().extract(bits - 1, 0) == 0;
}

static pair<expr, expr> is_dereferenceable(const Pointer &p,
                                           const expr &bytes_off,
                                           const expr &bytes,
                                           unsigned align, bool iswrite) {
  expr block_sz = p.blockSize();
  expr offset = p.getOffset();

  // check that offset is within bounds and that arith doesn't overflow
  expr cond = (offset + bytes_off).sextOrTrunc(bits_size_t).ule(block_sz);
  cond &= offset.add_no_uoverflow(bytes_off);

  cond &= p.isBlockAlive();
  cond &= !p.isReadnone();

  if (iswrite)
    cond &= p.isWritable() && !p.isReadonly();

  // try some constant folding; these are implied by the conditions above
  if (bytes.ugt(block_sz).isTrue() ||
      offset.sextOrTrunc(bits_size_t).uge(block_sz).isTrue() ||
      isUndef(offset))
    cond = false;

  return { move(cond), p.isAligned(align) };
}

// When bytes is 0, pointer is always derefenceable
AndExpr Pointer::isDereferenceable(const expr &bytes0, unsigned align,
                                    bool iswrite) {
  expr bytes_off = bytes0.zextOrTrunc(bits_for_offset);
  expr bytes = bytes0.zextOrTrunc(bits_size_t);
  DisjointExpr<expr> UB(expr(false)), is_aligned(expr(false)), all_ptrs;

  for (auto &[ptr_expr, domain] : DisjointExpr<expr>(p, true, true)) {
    Pointer ptr(m, ptr_expr);
    auto [ub, aligned] = ::is_dereferenceable(ptr, bytes_off, bytes, align,
                                              iswrite);

    // record pointer if not definitely unfeasible
    if (!ub.isFalse() && !aligned.isFalse() && !ptr.blockSize().isZero())
      all_ptrs.add(ptr_expr, domain);

    UB.add(move(ub), domain);
    is_aligned.add(move(aligned), domain);
  }

  AndExpr exprs;
  exprs.add(bytes == 0 || *UB());

  // cannot store more bytes than address space
  if (bytes0.bits() > bits_size_t)
    exprs.add(bytes0.extract(bytes0.bits() - 1, bits_size_t) == 0);

  // address must be always aligned regardless of access size
  exprs.add(*is_aligned());

  // trim set of valid ptrs
  if (auto ptrs = all_ptrs())
    p = *ptrs;
  else
    p = expr::mkUInt(0, totalBits());

  return exprs;
}

AndExpr Pointer::isDereferenceable(unsigned bytes, unsigned align,
                                    bool iswrite) {
  return isDereferenceable(expr::mkUInt(bytes, bits_size_t), align, iswrite);
}

// general disjoint check for unsigned integer
// This function assumes that both begin + len don't overflow
static expr disjoint(const expr &begin1, const expr &len1, const expr &begin2,
                     const expr &len2) {
  return begin1.uge(begin2 + len2) || begin2.uge(begin1 + len1);
}

// This function assumes that both begin + len don't overflow
void Pointer::isDisjoint(const expr &len1, const Pointer &ptr2,
                           const expr &len2) const {
  m.state->addUB(getBid() != ptr2.getBid() ||
                  disjoint(getOffsetSizet(),
                           len1.zextOrTrunc(bits_size_t),
                           ptr2.getOffsetSizet(),
                           len2.zextOrTrunc(bits_size_t)));
}

expr Pointer::isBlockAlive() const {
  // If programs have no free(), we assume all blocks are always live.
  // For non-local blocks, there's enough non-determinism through block size,
  // that can be 0 or non-0
  if (!has_free && !has_dead_allocas)
    return true;

  // globals are always live
  static_assert(GLOBAL == 0);
  if (getAllocType().isZero())
    return true;

  // NULL block is dead
  if (has_null_block && getBid().isZero())
    return false;

  auto bid = getShortBid();
  return mkIf_fold(isLocal(), load_bv(m.local_block_liveness, bid),
                   load_bv(m.non_local_block_liveness, bid));
}

expr Pointer::getAllocType() const {
  // If programs have no malloc & free, we don't need to store this information
  // since it is only used to check if free/delete is ok and
  // for memory refinement of local malloc'ed blocks
  if (!has_malloc && !has_free && !has_alloca)
    return expr::mkUInt(GLOBAL, 2);

  // if malloc is used, but no free, we can still ignore info for non-locals
  if (!has_free) {
    FunctionExpr non_local;
    non_local.add(getShortBid(), expr::mkUInt(GLOBAL, 2));
    return getValue("blk_kind", m.local_blk_kind, non_local, expr());

  }
  return getValue("blk_kind", m.local_blk_kind, m.non_local_blk_kind,
                   expr::mkUInt(0, 2));
}

expr Pointer::isHeapAllocated() const {
  assert(MALLOC == 2 && CXX_NEW == 3);
  return getAllocType().extract(1, 1) == 1;
}

expr Pointer::refined(const Pointer &other) const {
  // This refers to a block that was malloc'ed within the function
  expr local = getAllocType() == other.getAllocType();
  local &= blockSize() == other.blockSize();
  local &= getOffset() == other.getOffset();
  // Attributes are ignored at refinement.

  // TODO: this induces an infinite loop
  //local &= block_refined(other);

  return isBlockAlive().implies(
           other.isBlockAlive() &&
             expr::mkIf(isLocal(), isHeapAllocated().implies(local),
                        *this == other));
}

expr Pointer::fninputRefined(const Pointer &other, bool is_byval_arg) const {
  expr size = blockSize();
  expr off = getOffsetSizet();
  expr size2 = other.blockSize();
  expr off2 = other.getOffsetSizet();

  expr local
    = expr::mkIf(isHeapAllocated(),
                 other.isHeapAllocated() && off == off2 && size2.uge(size),

                 // must maintain same dereferenceability before & after
                 expr::mkIf(off.sle(-1),
                            off == off2 && size2.uge(size),
                            off2.sge(0) &&
                              expr::mkIf(off.sle(size),
                                         off2.sle(size2) && off2.uge(off) &&
                                           (size2 - off2).uge(size - off),
                                         off2.sgt(size2) && off == off2 &&
                                           size2.uge(size))));
  local = (other.isLocal() || other.isByval() || is_byval_arg) && local;

  // TODO: this induces an infinite loop
  // block_refined(other);

  return isBlockAlive().implies(
           other.isBlockAlive() &&
             expr::mkIf(isLocal(), local, *this == other));
}

expr Pointer::blockValRefined(const Pointer &other) const {
  if (m.non_local_block_val.eq(other.m.non_local_block_val))
    return true;

  Byte val(m, m.non_local_block_val.load(shortPtr()));
  Byte val2(other.m, other.m.non_local_block_val.load(other.shortPtr()));

  // refinement if offset had non-ptr value
  expr np1 = val.nonptrNonpoison();
  expr np2 = val2.nonptrNonpoison();
  bool np_eqs = np1.eq(np2);
  expr int_cnstr = does_sub_byte_access
                     ? (np2 | np1) == np1 &&
                       (val.nonptrValue() | np1) == (val2.nonptrValue() | np1)
                     : (np_eqs ? true : np2 == 0) &&
                       val.nonptrValue() == val2.nonptrValue();

  // fast path: if we didn't do any ptr store, then all ptrs in memory were
  // already there and don't need checking
  expr is_ptr = val.isPtr();
  expr is_ptr2 = val2.isPtr();
  expr ptr_cnstr;
  if (!does_ptr_store || is_ptr.isFalse() || is_ptr2.isFalse()) {
    ptr_cnstr = val == val2;
  } else {
    ptr_cnstr = val2.ptrNonpoison() &&
                val.ptrByteoffset() == val2.ptrByteoffset() &&
                val.ptr().refined(val2.ptr());
  }
  return val.isPoison() ||
         expr::mkIf(is_ptr == is_ptr2,
                    expr::mkIf(is_ptr, ptr_cnstr, int_cnstr),
                    // allow null ptr <-> zero
                    val.isZero() && !val2.isPoison() && val2.isZero());
}

expr Pointer::blockRefined(const Pointer &other) const {
  expr blk_size = blockSize();
  expr val_refines(true);
  uint64_t bytes;
  auto bytes_per_byte = bits_byte / 8;

  if (blk_size.isUInt(bytes) && (bytes / bytes_per_byte) <= 8) {
    expr bid = getBid();
    expr ptr_offset = getOffset();

    for (unsigned off = 0; off < bytes; off += bytes_per_byte) {
      expr off_expr = expr::mkUInt(off, bits_for_offset);
      Pointer p(m, bid, off_expr);
      Pointer q(other.m, p());
      val_refines &= (ptr_offset == off_expr).implies(p.blockValRefined(q));
    }
  } else {
    val_refines = blockValRefined(other);
  }

  expr alive = isBlockAlive();
  return alive == other.isBlockAlive() &&
         blk_size == other.blockSize() &&
         getAllocType() == other.getAllocType() &&
         isWritable() == other.isWritable() &&
         m.state->simplifyWithAxioms(
           blockAlignment().ule(other.blockAlignment())) &&
         (alive && getOffsetSizet().ult(blk_size)).implies(val_refines);
}

expr Pointer::isWritable() const {
  auto this_bid = getShortBid();
  expr non_local(true);
  for (auto bid : m.non_local_blk_nonwritable) {
    non_local &= this_bid != bid;
  }
  return isLocal() || non_local;
}

expr Pointer::isByval() const {
  auto this_bid = getShortBid();
  expr non_local(false);
  for (auto bid : m.byval_blks) {
    non_local |= this_bid == bid;
  }
  return !isLocal() && non_local;
}

expr Pointer::isNocapture() const {
  if (!has_nocapture)
    return false;

  // local pointers can't be no-capture
  if (isLocal().isTrue())
    return false;

  return p.extract(0, 0) == 1;
}

expr Pointer::isReadonly() const {
  if (!has_readonly)
    return false;
  return p.extract(has_nocapture, has_nocapture) == 1;
}

expr Pointer::isReadnone() const {
  if (!has_readnone)
    return false;
  unsigned idx = (unsigned)has_nocapture + (unsigned)has_readonly;
  return p.extract(idx, idx) == 1;
}

void Pointer::stripAttrs() {
  p = p.extract(totalBits()-1, bits_for_ptrattrs)
       .concat_zeros(bits_for_ptrattrs);
}

Pointer Pointer::mkNullPointer(const Memory &m) {
  // Null pointer exists if either source or target uses it.
  assert(has_null_block);
  // A null pointer points to block 0 without any attribute.
  return { m, 0, false };
}

expr Pointer::isNull() const {
  if (!has_null_block)
    return false;
  return *this == mkNullPointer(m);
}

expr Pointer::isNonZero() const {
  if (observes_addresses())
    return getAddress() != 0;
  return !isNull();
}

vector<expr>
Pointer::extractPossibleLocalBids(Memory &m, const expr &ptr_value) {
  vector<expr> ret;
  expr zero = expr::mkUInt(0, bits_for_offset);
  for (auto ptr_val : expr::allLeafs(ptr_value)) {
    for (auto bid : expr::allLeafs(Pointer(m, move(ptr_val)).getBid())) {
      Pointer ptr(m, bid, zero);
      if (!ptr.isLocal().isFalse())
        ret.emplace_back(ptr.getShortBid());
    }
  }
  return ret;
}

ostream& operator<<(ostream &os, const Pointer &p) {
  if (p.isNull().isTrue())
    return os << "null";

#define P(field, fn)   \
  if (field.isConst()) \
    field.fn(os);      \
  else                 \
    os << field

  os << "pointer(" << (p.isLocal().isTrue() ? "local" : "non-local")
     << ", block_id=";
  P(p.getBid(), printUnsigned);

  os << ", offset=";
  P(p.getOffset(), printSigned);

  if (bits_for_ptrattrs && !p.getAttrs().isZero()) {
    os << ", attrs=";
    P(p.getAttrs(), printUnsigned);
  }
#undef P
  return os << ')';
}


unsigned Memory::numLocals() const {
  return state->isSource() ? num_locals_src : num_locals_tgt;
}

unsigned Memory::numNonlocals() const {
  return state->isSource() ? num_nonlocals_src : num_nonlocals;
}

void Memory::store(const Pointer &p, const expr &val, expr &local,
                   expr &non_local) {
  if (numLocals() > 0) {
    Byte byte(*this, expr(val));
    if (byte.isPtr().isTrue())
      escapeLocals(Pointer::extractPossibleLocalBids(*this, byte.ptrValue()));
  }
  auto is_local = p.isLocal();
  auto idx = p.shortPtr();
  local = expr::mkIf(is_local, local.store(idx, val), local);
  non_local = expr::mkIf(!is_local, non_local.store(idx, val), non_local);
}

static void store_lambda(const Pointer &p, const expr &cond, const expr &val,
                         expr &local, expr &non_local) {
  auto is_local = p.isLocal();
  auto idx = p.shortPtr();

  if (!is_local.isFalse())
    local = expr::mkLambda({ idx },
              expr::mkIf(is_local && cond, val, local.load(idx)));

  if (!is_local.isTrue())
    non_local = expr::mkLambda({ idx },
                  expr::mkIf(!is_local && cond, val, non_local.load(idx)));
}

static expr load(const Pointer &p, const expr &local, const expr &non_local) {
  auto idx = p.shortPtr();
  return mkIf_fold(p.isLocal(), local.load(idx), non_local.load(idx));
}

static unsigned last_local_bid = 0;
// Global block id 0 is reserved for a null block if has_null_block is true.
// State::resetGlobals() sets last_nonlocal_bid to has_null_block.
static unsigned last_nonlocal_bid = 0;

static bool memory_unused() {
  return num_locals_src == 0 && num_locals_tgt == 0 && num_nonlocals == 0;
}

static expr mk_block_val_array() {
  return expr::mkArray("blk_val",
                       expr::mkUInt(0, Pointer::totalBitsShort()),
                       expr::mkUInt(0, Byte::bitsByte()));
}

static expr mk_liveness_array() {
  // consider all non_locals are initially alive
  // block size can still be 0 to invalidate accesses
  return num_nonlocals
           ? (expr::mkInt(-1, num_nonlocals) << expr::mkUInt(1, num_nonlocals))
           : expr();
}

static void mk_nonlocal_val_axioms(State &s, Memory &m, expr &val) {
  if (!does_ptr_mem_access || m.numNonlocals() == 0)
    return;

  auto idx = Pointer(m, "#idx", false, false).shortPtr();
#if 0
  expr is_ptr = does_int_mem_access
                  ? expr::mkUF("blk_init_isptr", { idx }, true)
                  : true;
  expr int_val
    = Byte::mkNonPtrByte(m, expr::mkUF("blk_init_nonptr", { idx },
                                       expr::mkUInt(0, bits_byte * 2)))();

  expr np = expr::mkUF("blk_init_ptr_np", { idx }, expr::mkUInt(0, 1));
  expr bid_off = expr::mkUF("blk_init_ptr_bid_off", { idx },
                   expr::mkUInt(0, bits_shortbid() + bits_for_offset + 3));
  bid_off = prepend_if(expr::mkUInt(0, 1), move(bid_off),
                       ptr_has_local_bit());
  Byte ptr_val = Byte::mkPtrByte(m, np.concat(bid_off));
  assert(ptr_val.ptr().isLocal().isFalse());

  val = expr::mkLambda({ idx }, expr::mkIf(is_ptr, ptr_val(), int_val));

  s.addAxiom(
    expr::mkForAll({ idx },
                   ptr_val.ptr().getShortBid().ule(m.numNonlocals() - 1)));
#else
  Byte byte(m, val.load(idx));
  Pointer loadedptr = byte.ptr();
  expr bid = loadedptr.getShortBid();
  s.addAxiom(
    expr::mkForAll({ idx },
      byte.isPtr().implies(!loadedptr.isLocal() &&
                            !loadedptr.isNocapture() &&
                            bid.ule(m.numNonlocals() - 1))));
#endif
}

Memory::Memory(State &state) : state(&state) {
  if (memory_unused())
    return;

  non_local_block_val = mk_block_val_array();
  non_local_block_liveness = mk_liveness_array();

  // Non-local blocks cannot initially contain pointers to local blocks
  // and no-capture pointers.
  mk_nonlocal_val_axioms(state, *this, non_local_block_val);

  // initialize all local blocks as non-pointer, poison value
  // This is okay because loading a pointer as non-pointer is also poison.
  local_block_val
    = expr::mkConstArray(expr::mkUInt(0, Pointer::totalBitsShort()),
                         Byte::mkPoisonByte(*this)());

  // all local blocks are dead in the beginning
  local_block_liveness = expr::mkUInt(0, numLocals());

  // A memory space is separated into non-local area / local area.
  // Non-local area is the lower half of memory (to include null pointer),
  // local area is the upper half.
  // This helps efficient encoding of disjointness between local and nonlocal
  // blocks.
  // The last byte of the memory cannot be allocated because ptr + size
  // (where size is the size of the block and ptr is the beginning address of
  // the block) should not overflow.

  // Initialize a memory block for null pointer.
  if (has_null_block)
    alloc(expr::mkUInt(0, bits_size_t), bits_program_pointer, GLOBAL, false,
          false, 0);

  escaped_local_blks.resize(numLocals(), false);

  assert(bits_for_offset <= bits_size_t);
}

void Memory::finishInitialization() {
  initial_non_local_block_val = non_local_block_val;
}

void Memory::mkAxioms(const Memory &other) const {
  assert(state->isSource() && !other.state->isSource());
  if (memory_unused())
    return;

  // transformation can increase alignment
  unsigned align = heap_block_alignment;
  for (unsigned bid = has_null_block; bid < num_nonlocals; ++bid) {
    Pointer p(*this, bid, false);
    Pointer q(other, bid, false);
    auto p_align = p.blockAlignment();
    auto q_align = q.blockAlignment();
    state->addAxiom(expr::mkIf(p.isHeapAllocated(),
                               p_align == align && q_align == align,
                               p_align.ule(q_align)));
  }

  if (!observes_addresses())
    return;

  if (has_null_block)
    state->addAxiom(Pointer::mkNullPointer(*this).getAddress(false) == 0);

  // Non-local blocks are disjoint.
  // Ignore null pointer block
  for (unsigned bid = has_null_block; bid < num_nonlocals; ++bid) {
    Pointer p1(*this, bid, false);
    auto addr = p1.getAddress();
    auto sz = p1.blockSize();

    expr disj = addr != 0;

    // Ensure block doesn't spill to local memory
    auto bit = bits_size_t - 1;
    disj &= (addr + sz).extract(bit, bit) == 0;

    // disjointness constraint
    for (unsigned bid2 = bid + 1; bid2 < num_nonlocals; ++bid2) {
      Pointer p2(*this, bid2, false);
      disj &= p2.isBlockAlive()
                .implies(disjoint(addr, sz, p2.getAddress(), p2.blockSize()));
    }
    state->addAxiom(p1.isBlockAlive().implies(disj));
  }

  // ensure locals fit in their reserved space
  expr one = expr::mkUInt(1, bits_size_t - 1);
  auto locals_fit = [&one](const Memory &m) {
    auto sum = expr::mkUInt(0, bits_size_t - 1);
    for (unsigned bid = 0, nlocals = m.numLocals(); bid < nlocals; ++bid) {
      Pointer p(m, bid, true);
      if (auto sz = m.local_blk_size.lookup(p.getShortBid())) {
        auto size = sz->extract(bits_size_t - 2, 0);
        auto align = one << p.blockAlignment().zextOrTrunc(bits_size_t - 1);
        align = align - one;
        auto sz_align = size + align;
        m.state->addOOM(size.add_no_uoverflow(align));
        m.state->addOOM(sum.add_no_uoverflow(sz_align));
        sum = sum + sz_align;
      }
    }
  };
  locals_fit(*this);
  locals_fit(other);
}

void Memory::resetBids(unsigned last_nonlocal) {
  last_nonlocal_bid = last_nonlocal;
  last_local_bid = 0;
  ptr_next_idx = 0;
}

void Memory::markByVal(unsigned bid) {
  byval_blks.emplace_back(bid);
}

expr Memory::mkInput(const char *name, const ParamAttrs &attrs) const {
  Pointer p(*this, name, false, false, false, attr_to_bitvec(attrs));
  if (attrs.has(ParamAttrs::NonNull))
    state->addAxiom(p.isNonZero());
  state->addAxiom(p.getShortBid().ule(numNonlocals() - 1));

  return p.release();
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
  Pointer p(*this, expr::mkUInt(0, bits_for_bid), offset,
            attr_to_bitvec(attrs));
  return { p.release(), move(undef) };
}

pair<expr,expr>
Memory::mkFnRet(const char *name,
                const vector<PtrInput> &ptr_inputs) const {
  expr var
    = expr::mkFreshVar(name, expr::mkUInt(0, bits_for_bid + bits_for_offset));
  Pointer p(*this, var.concat_zeros(bits_for_ptrattrs));

  set<expr> local;
  for (auto &in : ptr_inputs) {
    // TODO: callee cannot observe the bid if this is byval.
    Pointer inp(*this, in.val.value);
    if (!inp.isLocal().isFalse())
      local.emplace(in.val.non_poison && p.getBid() == inp.getBid());
  }
  for (unsigned i = 0; i < numLocals(); ++i) {
    if (escaped_local_blks[i])
      local.emplace(p.getShortBid() == i);
  }

  state->addAxiom(expr::mkIf(p.isLocal(),
                             expr::mk_or(local),
                             p.getShortBid().ule(numNonlocals() - 1)));
  return { p.release(), move(var) };
}

expr Memory::CallState::implies(const CallState &st) const {
  if (empty || st.empty)
    return true;
  // NOTE: using equality here is an approximation.
  // TODO: benchmark using quantifiers to state implication
  expr ret = nonlocal_val_var == st.nonlocal_val_var;

  if (nonlocal_liveness_var.isValid() && st.nonlocal_liveness_var.isValid())
    ret &= nonlocal_liveness_var == st.nonlocal_liveness_var;

  // TODO: This should align local block ids
  // TOOD 2: This should compare local blocks that are escaped only.
  // Relevant test: alive-tv/bugs/pr10067.srctgt.ll
  ret &= local_val_var == st.local_val_var;
  return ret;
}

ostream &operator<<(ostream &os, const Memory::CallState &cstate) {
  if (cstate.empty) {
    os << "(EMPTY)\n";
    return os;
  }

  os << "<Call State>\n"
     << "- nonlocal val: " << cstate.nonlocal_val << "\n"
     << "- nonlocal liveness: " << cstate.nonlocal_liveness << "\n"
     << "- local val: " << cstate.local_val << "\n";
  return os;
}

Memory::CallState
Memory::mkCallState(const vector<PtrInput> *ptr_inputs, bool nofree) const {
  CallState st;
  st.empty = false;

  auto blk_val = mk_block_val_array();
  st.nonlocal_val_var = expr::mkFreshVar("blk_val", blk_val);
  st.local_val_var = expr::mkFreshVar("localblk_val", blk_val);

  {
    Pointer p(*this, "#idx", false);
    Pointer p_local(*this, "#idx", true);
    expr modifies(true), modifies_local(true);

    if (ptr_inputs) {
      modifies = false;
      for (auto &[arg, is_byval_arg, is_nocapture_arg] : *ptr_inputs) {
        (void)is_nocapture_arg;
        Pointer argp(*this, arg.value);
        modifies |= arg.non_poison && argp.getBid() == p.getBid();
        if (!is_byval_arg)
          modifies_local |= arg.non_poison &&
                            argp.getBid() == p_local.getBid();
      }
    }

    auto idx = p.shortPtr();
    expr updated_mem
      = initial_non_local_block_val.subst(blk_val, st.nonlocal_val_var);
    st.nonlocal_val
      = expr::mkLambda(
          { idx },
          expr::mkIf(modifies, updated_mem.load(idx),
                     non_local_block_val.load(idx)));

    auto idx_local = p_local.shortPtr();
    st.local_val
      = expr::mkLambda(
          { idx_local },
          expr::mkIf(modifies_local && isEscapedLocal(idx_local),
                     st.local_val_var.load(idx_local),
                     local_block_val.load(idx_local)));
  }

  if (num_nonlocals_src && !nofree) {
    expr one  = expr::mkUInt(1, num_nonlocals);
    expr zero = expr::mkUInt(0, num_nonlocals);
    expr mask = has_null_block ? one : zero;
    for (unsigned bid = has_null_block; bid < num_nonlocals; ++bid) {
      expr ok_arg = true;
      if (ptr_inputs) {
        for (auto &[arg, is_byval_arg, is_nocapture_arg] : *ptr_inputs) {
          (void)is_nocapture_arg;
          if (!is_byval_arg)
            ok_arg &= !arg.non_poison ||
                      Pointer(*this, arg.value).getBid() != bid;
        }
      }
      expr heap = Pointer(*this, bid, false).isHeapAllocated();
      mask = mask | expr::mkIf(heap && ok_arg,
                               zero,
                               one << expr::mkUInt(bid, num_nonlocals));
    }

    if (mask.isAllOnes()) {
      st.nonlocal_liveness = non_local_block_liveness;
    } else {
      st.nonlocal_liveness_var
        = expr::mkFreshVar("blk_liveness", mk_liveness_array());
      // functions can free an object, but cannot bring a dead one back to live
      st.nonlocal_liveness
        = non_local_block_liveness & (st.nonlocal_liveness_var | mask);
    }
  }
  return st;
}

void Memory::setState(const Memory::CallState &st) {
  non_local_block_val = st.nonlocal_val;
  local_block_val = st.local_val;
  non_local_block_liveness = st.nonlocal_liveness;
  mk_nonlocal_val_axioms(*state, *this, non_local_block_val);
}

static expr disjoint_local_blocks(const Memory &m, const expr &addr,
                                  const expr &sz, FunctionExpr &blk_addr) {
  expr disj = true;

  // Disjointness of block's address range with other local blocks
  auto one = expr::mkUInt(1, 1);
  auto zero = expr::mkUInt(0, bits_for_offset);
  for (auto &[sbid, addr0] : blk_addr) {
    (void)addr0;
    Pointer p2(m, prepend_if(one, expr(sbid), ptr_has_local_bit()), zero);
    disj &= p2.isBlockAlive()
              .implies(disjoint(addr, sz, p2.getAddress(), p2.blockSize()));
  }
  return disj;
}

pair<expr, expr>
Memory::alloc(const expr &size, unsigned align, BlockKind blockKind,
              const expr &precond, const expr &nonnull,
              optional<unsigned> bidopt, unsigned *bid_out) {
  assert(!memory_unused());

  // Produce a local block if blockKind is heap or stack.
  bool is_local = blockKind != GLOBAL && blockKind != CONSTGLOBAL;

  auto &last_bid = is_local ? last_local_bid : last_nonlocal_bid;
  unsigned bid = bidopt ? *bidopt : last_bid;
  assert((is_local && bid < numLocals()) ||
         (!is_local && bid < numNonlocals()));
  if (!bidopt)
    ++last_bid;
  assert(bid < last_bid);

  if (bid_out)
    *bid_out = bid;

  expr size_zext = size.zextOrTrunc(bits_size_t);
  expr nooverflow = size_zext.extract(bits_size_t - 1, bits_size_t - 1) == 0;
  assert(bits_byte == 8 || is_local ||
         size_zext.urem(expr::mkUInt(bits_byte/8, bits_size_t)).isZero());

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

  if (is_local) {
    if (observes_addresses()) {
      // MSB of local block area's address is 1.
      auto addr_var
        = expr::mkFreshVar("local_addr",
                           expr::mkUInt(0, bits_size_t - align_bits - 1));
      state->addQuantVar(addr_var);

      expr blk_addr = addr_var.concat_zeros(align_bits);
      auto full_addr = expr::mkUInt(1, 1).concat(blk_addr);

      // addr + size does not overflow
      if (!size.uge(align).isFalse())
        state->addPre(allocated.implies(full_addr.add_no_uoverflow(size_zext)));

      // Disjointness of block's address range with other local blocks
      state->addPre(
        allocated.implies(disjoint_local_blocks(*this, full_addr, size_zext,
                                                local_blk_addr)));

      local_blk_addr.add(short_bid, move(blk_addr));
    }
  } else {
    state->addAxiom(p.blockSize() == size_zext);
    state->addAxiom(p.isBlockAligned(align, true));
    state->addAxiom(p.getAllocType() == alloc_ty);

    if (align_bits && observes_addresses())
      state->addAxiom(p.getAddress().extract(align_bits - 1, 0) == 0);

    if (blockKind == CONSTGLOBAL)
      non_local_blk_nonwritable.emplace(bid);
  }

  store_bv(p, allocated, local_block_liveness, non_local_block_liveness);
  (is_local ? local_blk_size : non_local_blk_size)
    .add(short_bid, size_zext.trunc(bits_size_t - 1));
  (is_local ? local_blk_align : non_local_blk_align)
    .add(short_bid, expr::mkUInt(align_bits, 8));
  (is_local ? local_blk_kind : non_local_blk_kind)
    .add(short_bid, expr::mkUInt(alloc_ty, 2));

  if (nonnull.isTrue())
    return { p.release(), move(allocated) };

  expr nondet_nonnull = expr::mkFreshVar("#alloc_nondet_nonnull", true);
  state->addQuantVar(nondet_nonnull);
  allocated = precond && (nonnull || (nooverflow && nondet_nonnull));
  return { expr::mkIf(allocated, p(), Pointer::mkNullPointer(*this)()),
           move(allocated) };
}

void Memory::startLifetime(const expr &ptr_local) {
  assert(!memory_unused());
  Pointer p(*this, ptr_local);
  state->addUB(p.isLocal());

  if (observes_addresses())
    state->addPre(disjoint_local_blocks(*this, p.getAddress(), p.blockSize(),
                  local_blk_addr));

  store_bv(p, true, local_block_liveness, non_local_block_liveness, true);
}

void Memory::free(const expr &ptr, bool unconstrained) {
  assert(!memory_unused() && (has_free || has_dead_allocas));
  Pointer p(*this, ptr);
  if (!unconstrained)
    state->addUB(p.isNull() || (p.getOffset() == 0 &&
                                p.isBlockAlive() &&
                                p.getAllocType() == Pointer::MALLOC));
  store_bv(p, false, local_block_liveness, non_local_block_liveness);
}

unsigned Memory::getStoreByteSize(const Type &ty) {
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

void Memory::store(const expr &p, const StateValue &v, const Type &type,
                   unsigned align, const set<expr> &undef_vars0,
                   bool deref_check) {
  assert(!memory_unused());
  Pointer ptr(*this, p);
  unsigned bytesz = bits_byte / 8;

  undef_vars.insert(undef_vars0.begin(), undef_vars0.end());

  if (deref_check)
    state->addUB(ptr.isDereferenceable(getStoreByteSize(type), align,
                                       !state->isInitializationPhase()));

  auto aty = type.getAsAggregateType();
  if (aty && !isNonPtrVector(type)) {
    unsigned byteofs = 0;
    for (unsigned i = 0, e = aty->numElementsConst(); i < e; ++i) {
      auto &child = aty->getChild(i);
      if (child.bits() == 0)
        continue;
      auto ptr_i = ptr + byteofs;
      auto align_i = gcd(align, byteofs % align);
      store(ptr_i(), aty->extract(v, i), child, align_i, {}, false);
      byteofs += getStoreByteSize(child);
    }
    assert(byteofs == getStoreByteSize(type));

  } else {
    vector<Byte> bytes = valueToBytes(v, type, *this, state);
    assert(!v.isValid() || bytes.size() * bytesz == getStoreByteSize(type));
    assert(align % bytesz == 0);

    for (unsigned i = 0, e = bytes.size(); i < e; ++i) {
      auto ptr_i = ptr + (little_endian ? i * bytesz :
                                          (e - i - 1) * bytesz);
      store(ptr_i, bytes[i](), local_block_val, non_local_block_val);
    }
  }
}

pair<StateValue, AndExpr>
Memory::load(const expr &p, const Type &type, unsigned align) {
  assert(!memory_unused());
  unsigned bytesz = bits_byte / 8;
  unsigned bytecount = getStoreByteSize(type);

  Pointer ptr(*this, p);
  auto ubs = ptr.isDereferenceable(bytecount, align, false);

  StateValue ret;
  auto aty = type.getAsAggregateType();
  if (aty && !isNonPtrVector(type)) {
    vector<StateValue> member_vals;
    unsigned byteofs = 0;
    for (unsigned i = 0, e = aty->numElementsConst(); i < e; ++i) {
      auto ptr_i = ptr + byteofs;
      auto align_i = gcd(align, byteofs % align);
      member_vals.push_back(load(ptr_i(), aty->getChild(i), align_i).first);
      byteofs += getStoreByteSize(aty->getChild(i));
    }
    assert(byteofs == bytecount);
    ret = aty->aggregateVals(member_vals);

  } else {
    assert(align % bytesz == 0);
    vector<Byte> loadedBytes;
    bytecount /= bytesz;
    for (unsigned i = 0; i < bytecount; ++i) {
      auto ptr_i = ptr + (little_endian ? i * bytesz
                                        : (bytecount - i - 1) * bytesz);
      loadedBytes.emplace_back(*this, ::load(ptr_i, local_block_val,
                                             non_local_block_val));
    }
    ret = bytesToValue(*this, loadedBytes, type);
  }
  return { state->rewriteUndef(move(ret), undef_vars), move(ubs) };
}

Byte Memory::load(const Pointer &p) const {
  return { *this, ::load(p, local_block_val, non_local_block_val) };
}

// idx in [ptr, ptr+sz)
static expr ptr_deref_within(const Pointer &idx, const Pointer &ptr,
                             const expr &size) {
  expr ret = idx.getShortBid() == ptr.getShortBid();
  ret &= idx.getOffset().uge(ptr.getOffset());

  if (!size.zextOrTrunc(bits_size_t).uge(ptr.blockSize()).isTrue())
    ret &= idx.getOffset().ult((ptr + size).getOffset());

  return ret;
}

void Memory::memset(const expr &p, const StateValue &val, const expr &bytesize,
                    unsigned align, const set<expr> &undef_vars0) {
  assert(!memory_unused());
  assert(!val.isValid() || val.bits() == 8);
  unsigned bytesz = bits_byte / 8;
  Pointer ptr(*this, p);
  state->addUB(ptr.isDereferenceable(bytesize, align, true));
  undef_vars.insert(undef_vars0.begin(), undef_vars0.end());

  auto wval = val;
  for (unsigned i = 1; i < bytesz; ++i) {
    wval = wval.concat(val);
  }
  assert(!val.isValid() || wval.bits() == bits_byte);

  auto bytes = valueToBytes(wval, IntType("", bits_byte), *this, state);
  assert(bytes.size() == 1);

  uint64_t n;
  if (bytesize.isUInt(n) && (n / bytesz) <= 4) {
    for (unsigned i = 0; i < n; i += bytesz) {
      store(ptr + i, bytes[0](), local_block_val, non_local_block_val);
    }
  } else {
    Pointer idx(*this, "#idx", ptr.isLocal());
    store_lambda(idx, ptr_deref_within(idx, ptr, bytesize), bytes[0](),
                 local_block_val, non_local_block_val);
  }
}

void Memory::memcpy(const expr &d, const expr &s, const expr &bytesize,
                    unsigned align_dst, unsigned align_src, bool is_move) {
  assert(!memory_unused());
  unsigned bytesz = bits_byte / 8;

  Pointer dst(*this, d), src(*this, s);
  state->addUB(dst.isDereferenceable(bytesize, align_dst, true));
  state->addUB(src.isDereferenceable(bytesize, align_src, false));
  if (!is_move)
    src.isDisjoint(bytesize, dst, bytesize);

  // copy to itself
  if ((src == dst).isTrue())
    return;

  uint64_t n;
  if (bytesize.isUInt(n) && (n / bytesz) <= 4) {
    auto old_local = local_block_val, old_nonlocal = non_local_block_val;
    for (unsigned i = 0; i < n; i += bytesz) {
      store(dst + i, ::load(src + i, old_local, old_nonlocal),
            local_block_val, non_local_block_val);
    }
  } else {
    Pointer dst_idx(*this, "#idx", dst.isLocal());
    Pointer src_idx = src + (dst_idx.getOffset() - dst.getOffset());
    store_lambda(dst_idx, ptr_deref_within(dst_idx, dst, bytesize),
                 ::load(src_idx, local_block_val, non_local_block_val),
                 local_block_val, non_local_block_val);
  }
}

expr Memory::ptr2int(const expr &ptr) const {
  assert(!memory_unused());
  return Pointer(*this, ptr).getAddress();
}

expr Memory::int2ptr(const expr &val) const {
  assert(!memory_unused());
  // TODO
  return {};
}

pair<expr,Pointer>
Memory::refined(const Memory &other, bool skip_constants,
                const vector<PtrInput> *set_ptrs) const {
  if (num_nonlocals <= has_null_block)
    return { true, Pointer(*this, expr()) };

  assert(!memory_unused());
  Pointer ptr(*this, "#idx_refinement", false);
  expr ptr_bid = ptr.getBid();
  expr offset = ptr.getOffset();
  expr ret(true);

  auto is_constglb = [](const Memory &m, unsigned bid) {
    return
      find(m.non_local_blk_nonwritable.begin(),
           m.non_local_blk_nonwritable.end(), bid) !=
        m.non_local_blk_nonwritable.end();
  };

  for (unsigned bid = has_null_block; bid < num_nonlocals_src; ++bid) {
    if (skip_constants && is_constglb(*this, bid)) {
      assert(is_constglb(other, bid));
      continue;
    }
    expr bid_expr = expr::mkUInt(bid, bits_for_bid);
    Pointer p(*this, bid_expr, offset);
    Pointer q(other, p());
    if ((p.isByval() && q.isByval()).isTrue())
      continue;
    ret &= (ptr_bid == bid_expr).implies(p.blockRefined(q));
  }

  // restrict refinement check to set of request blocks
  if (set_ptrs) {
    expr c(false);
    for (auto &itm: *set_ptrs) {
      // TODO: deal with the byval arg case (itm.second)
      auto &ptr = itm.val;
      c |= ptr.non_poison && Pointer(*this, ptr.value).getBid() == ptr_bid;
    }
    ret = c.implies(ret);
  }

  return { move(ret), move(ptr) };
}

expr Memory::checkNocapture() const {
  if (!does_ptr_store)
    return true;

  auto name = local_name(state, "#offset_nocapture");
  auto ofs = expr::mkVar(name.c_str(), bits_for_offset);
  expr res(true);

  for (unsigned bid = has_null_block; bid < numNonlocals(); ++bid) {
    Pointer p(*this, expr::mkUInt(bid, bits_for_bid), ofs);
    Byte b(*this, non_local_block_val.load(p.shortPtr()));
    Pointer loadp(*this, b.ptrValue());
    res &= p.isBlockAlive().implies(
             (b.isPtr() && b.ptrNonpoison()).implies(!loadp.isNocapture()));
  }
  if (!res.isTrue())
    state->addQuantVar(ofs);
  return res;
}

expr Memory::isEscapedLocal(const smt::expr &short_bid) const {
  expr e(false);
  for (unsigned bid0 = 0; bid0 < escaped_local_blks.size(); ++bid0) {
    if (escaped_local_blks[bid0])
      e |= short_bid == bid0;
  }
  return e;
}

void Memory::escapeLocals(vector<expr> &&local_bids) {
  uint64_t bid;
  for (const auto &bid_expr : local_bids) {
    if (bid_expr.isUInt(bid)) {
      if (bid < numLocals())
        escaped_local_blks[bid] = true;
    } else {
      // may escape a local ptr, but we don't know which one
      // TODO: check whether bid_expr cannot be some bids better
      escaped_local_blks.clear();
      escaped_local_blks.resize(numLocals(), true);
      break;
    }
  }
}

Memory Memory::mkIf(const expr &cond, const Memory &then, const Memory &els) {
  assert(then.state == els.state);
  Memory ret(then);
  ret.non_local_block_val      = expr::mkIf(cond, then.non_local_block_val,
                                            els.non_local_block_val);
  ret.local_block_val          = expr::mkIf(cond, then.local_block_val,
                                            els.local_block_val);
  ret.non_local_block_liveness = expr::mkIf(cond, then.non_local_block_liveness,
                                            els.non_local_block_liveness);
  ret.local_block_liveness     = expr::mkIf(cond, then.local_block_liveness,
                                            els.local_block_liveness);
  ret.local_blk_addr.add(els.local_blk_addr);
  ret.local_blk_size.add(els.local_blk_size);
  ret.local_blk_align.add(els.local_blk_align);
  ret.local_blk_kind.add(els.local_blk_kind);
  ret.non_local_blk_nonwritable.insert(els.non_local_blk_nonwritable.begin(),
                                       els.non_local_blk_nonwritable.end());
  ret.non_local_blk_size.add(els.non_local_blk_size);
  ret.non_local_blk_align.add(els.non_local_blk_align);
  ret.non_local_blk_kind.add(els.non_local_blk_kind);
  ret.byval_blks.insert(ret.byval_blks.end(), els.byval_blks.begin(),
                        els.byval_blks.end());
  for (unsigned i = 0, nlocals = then.numLocals(); i < nlocals; ++i) {
    if (els.escaped_local_blks[i])
      ret.escaped_local_blks[i] = true;
  }
  ret.undef_vars.insert(els.undef_vars.begin(), els.undef_vars.end());
  return ret;
}

bool Memory::operator<(const Memory &rhs) const {
  // FIXME: remove this once we move to C++20
  // NOTE: we don't compare field state so that memories from src/tgt can
  // compare equal
  return
    tie(non_local_block_val, local_block_val, initial_non_local_block_val,
        non_local_block_liveness, local_block_liveness, local_blk_addr,
        local_blk_size, local_blk_align, local_blk_kind,
        non_local_blk_nonwritable, non_local_blk_size, non_local_blk_align,
        non_local_blk_kind, byval_blks, escaped_local_blks, undef_vars) <
    tie(rhs.non_local_block_val, rhs.local_block_val,
        rhs.initial_non_local_block_val,
        rhs.non_local_block_liveness, rhs.local_block_liveness,
        rhs.local_blk_addr, rhs.local_blk_size, rhs.local_blk_align,
        rhs.local_blk_kind, rhs.non_local_blk_nonwritable,
        rhs.non_local_blk_size, rhs.non_local_blk_align,
        rhs.non_local_blk_kind, rhs.byval_blks, rhs.escaped_local_blks,
        rhs.undef_vars);
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
      P("align", expr::mkInt(1, 32) << p.blockAlignment().zextOrTrunc(32));
      P("alloc type", p.getAllocType());
      if (observes_addresses())
        P("address", p.getAddress());
      os << '\n';
    }
  };

  bool did_header = false;
  auto header = [&](const char *msg) {
    if (!did_header) {
      os << '\n'
         << (state->isSource() ? "SOURCE" : "TARGET")
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
  if (num_nonlocals > 0) os << "Non-local: " << m.nonlocal.simplify() << "\n\n"

ostream& operator<<(ostream &os, const Memory &m) {
  if (memory_unused())
    return os;
  os << "\n\nMEMORY\n======\n";
  P("BLOCK VALUE:", local_block_val, non_local_block_val);
  P("BLOCK LIVENESS:", local_block_liveness, non_local_block_liveness);
  P("BLOCK SIZE:", local_blk_size, non_local_blk_size);
  P("BLOCK ALIGN:", local_blk_align, non_local_blk_align);
  P("BLOCK KIND:", local_blk_kind, non_local_blk_kind);
  if (m.numLocals() > 0) {
    os << "ESCAPED LOCAL BLOCKS: ";
    for (unsigned i = 0, nlocals = m.numLocals(); i < nlocals; ++i) {
      os << m.escaped_local_blks[i];
    }
    os << "\nLOCAL BLOCK ADDR: " << m.local_blk_addr << '\n';
  }
  if (!m.non_local_blk_nonwritable.empty()) {
    os << "CONST NON-LOCALS:";
    for (auto bid : m.non_local_blk_nonwritable) {
      os << ' ' << bid;
    }
    os << '\n';
  }
  return os;
}

}
