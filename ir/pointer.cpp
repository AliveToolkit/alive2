// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/pointer.h"
#include "ir/memory.h"
#include "ir/globals.h"
#include "ir/state.h"
#include "util/compiler.h"

using namespace IR;
using namespace smt;
using namespace std;
using namespace util;

static unsigned ptr_next_idx;

static expr prepend_if(const expr &pre, expr &&e, bool prepend) {
  return prepend ? pre.concat(e) : move(e);
}

static string local_name(const State *s, const char *name) {
  return string(name) + (s->isSource() ? "_src" : "_tgt");
}

// Assumes that both begin + len don't overflow
static expr disjoint(const expr &begin1, const expr &len1, const expr &begin2,
                     const expr &len2) {
  return begin1.uge(begin2 + len2) || begin2.uge(begin1 + len1);
}

static bool has_local_bit() {
  return (num_locals_src || num_locals_tgt) && num_nonlocals;
}

static unsigned total_bits_short() {
  return Pointer::bitsShortBid() + Pointer::bitsShortOffset();
}

static expr attr_to_bitvec(const ParamAttrs &attrs) {
  if (!bits_for_ptrattrs)
    return {};

  uint64_t bits = 0;
  auto idx = 0;
  auto to_bit = [&](bool b, const ParamAttrs::Attribute &a) -> uint64_t {
    return b ? ((attrs.has(a) ? 1 : 0) << idx++) : 0;
  };
  bits |= to_bit(has_nocapture, ParamAttrs::NoCapture);
  bits |= to_bit(has_noread, ParamAttrs::NoRead);
  bits |= to_bit(has_nowrite, ParamAttrs::NoWrite);
  return expr::mkUInt(bits, bits_for_ptrattrs);
}

namespace IR {

Pointer::Pointer(const Memory &m, const expr &bid, const expr &offset,
                 const expr &attr) : m(m), p(bid.concat(offset)) {
  if (bits_for_ptrattrs)
    p = p.concat(attr);
  assert(!bid.isValid() || !offset.isValid() || p.bits() == totalBits());
}

Pointer::Pointer(const Memory &m, const char *var_name, const expr &local,
                 bool unique_name, bool align, const ParamAttrs &attr) : m(m) {
  string name = var_name;
  if (unique_name)
    name += '!' + to_string(ptr_next_idx++);

  unsigned bits = total_bits_short() + !align * zeroBitsShortOffset();
  p = prepend_if(local.toBVBool(),
                 expr::mkVar(name.c_str(), bits), has_local_bit());
  if (align)
    p = p.concat_zeros(zeroBitsShortOffset());
  if (bits_for_ptrattrs)
    p = p.concat(attr_to_bitvec(attr));
  assert(!local.isValid() || p.bits() == totalBits());
}

Pointer::Pointer(const Memory &m, expr repr) : m(m), p(move(repr)) {
  assert(!p.isValid() || p.bits() == totalBits());
}

Pointer::Pointer(const Memory &m, unsigned bid, bool local)
  : m(m), p(
    prepend_if(expr::mkUInt(local, 1),
               expr::mkUInt(bid, bitsShortBid())
                 .concat_zeros(bits_for_offset + bits_for_ptrattrs),
               has_local_bit())) {
  assert((local && bid < m.numLocals()) || (!local && bid < num_nonlocals));
  assert(p.bits() == totalBits());
}

Pointer::Pointer(const Memory &m, const expr &bid, const expr &offset,
                 const ParamAttrs &attr)
  : Pointer(m, bid, offset, attr_to_bitvec(attr)) {}

expr Pointer::mkLongBid(const expr &short_bid, bool local) {
  if (!has_local_bit())
    return short_bid;
  return expr::mkUInt(local, 1).concat(short_bid);
}

unsigned Pointer::totalBits() {
  return bits_for_ptrattrs + bits_for_bid + bits_for_offset;
}

unsigned Pointer::bitsShortBid() {
  return bits_for_bid - has_local_bit();
}

unsigned Pointer::bitsShortOffset() {
  return bits_for_offset - zeroBitsShortOffset();
}

unsigned Pointer::zeroBitsShortOffset() {
  assert(is_power2(bits_byte));
  return ilog2(bits_byte / 8);
}

expr Pointer::isLocal(bool simplify) const {
  if (m.numLocals() == 0)
    return false;
  if (m.numNonlocals() == 0)
    return true;

  auto bit = totalBits() - 1;
  expr local = p.extract(bit, bit);

  if (simplify && m.isInitialMemBlock(local))
    return false;

  return local == 1;
}

expr Pointer::getBid() const {
  return p.extract(totalBits() - 1, bits_for_offset + bits_for_ptrattrs);
}

expr Pointer::getShortBid() const {
  return p.extract(totalBits() - 1 - has_local_bit(),
                   bits_for_offset + bits_for_ptrattrs);
}

expr Pointer::getOffset() const {
  return p.extract(bits_for_offset + bits_for_ptrattrs - 1, bits_for_ptrattrs);
}

expr Pointer::getOffsetSizet() const {
  return getOffset().sextOrTrunc(bits_size_t);
}

expr Pointer::getShortOffset() const {
  return p.extract(bits_for_offset + bits_for_ptrattrs - 1,
                   bits_for_ptrattrs + zeroBitsShortOffset());
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
  assert(Memory::observesAddresses());

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
  for (auto &[ptr_expr, domain] : DisjointExpr<expr>(p, 3)) {
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
                   expr::mkUInt(0, 6), true);
}

expr Pointer::isBlockAligned(unsigned align, bool exact) const {
  if (!exact && align == 1)
    return true;

  auto bits = ilog2(align);
  expr blk_align = blockAlignment();
  return exact ? blk_align == bits : blk_align.uge(bits);
}

expr Pointer::isAligned(unsigned align) {
  if (align == 1)
    return true;

  auto offset = getOffset();
  if (isUndef(offset))
    return false;

  auto bits = min(ilog2(align), bits_for_offset);

  expr blk_align = isBlockAligned(align);

  if (!Memory::observesAddresses() ||
      (blk_align.isConst() && offset.isConst())) {
    // This is stricter than checking getAddress(), but as addresses are not
    // observed, program shouldn't be able to distinguish this from checking
    // getAddress()
    auto zero = expr::mkUInt(0, bits);
    auto newp = p.extract(totalBits() - 1, bits_for_ptrattrs + bits)
                 .concat(zero);
    if (bits_for_ptrattrs)
      newp = newp.concat(getAttrs());
    p = move(newp);
    assert(!p.isValid() || p.bits() == totalBits());
    return { blk_align && offset.extract(bits - 1, 0) == zero };
  }

  return getAddress().extract(bits - 1, 0) == 0;
}

static pair<expr, expr> is_dereferenceable(Pointer &p,
                                           const expr &bytes_off,
                                           const expr &bytes,
                                           unsigned align, bool iswrite) {
  expr block_sz = p.blockSize();
  expr offset = p.getOffset();

  // check that offset is within bounds and that arith doesn't overflow
  expr cond = (offset + bytes_off).sextOrTrunc(bits_size_t).ule(block_sz);
  cond &= offset.add_no_uoverflow(bytes_off);

  cond &= p.isBlockAlive();

  if (iswrite)
    cond &= p.isWritable() && !p.isNoWrite();
  else
    cond &= !p.isNoRead();

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

  for (auto &[ptr_expr, domain] : DisjointExpr<expr>(p, 3)) {
    Pointer ptr(m, ptr_expr);
    auto [ub, aligned] = ::is_dereferenceable(ptr, bytes_off, bytes, align,
                                              iswrite);

    // record pointer if not definitely unfeasible
    if (!ub.isFalse() && !aligned.isFalse() && !ptr.blockSize().isZero())
      all_ptrs.add(ptr.release(), domain);

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

AndExpr Pointer::isDereferenceable(uint64_t bytes, unsigned align,
                                   bool iswrite) {
  return isDereferenceable(expr::mkUInt(bytes, bits_size_t), align, iswrite);
}

// This function assumes that both begin + len don't overflow
void Pointer::isDisjointOrEqual(const expr &len1, const Pointer &ptr2,
                                const expr &len2) const {
  auto off = getOffsetSizet();
  auto off2 = ptr2.getOffsetSizet();
  m.state->addUB(getBid() != ptr2.getBid() ||
                 off == off2 ||
                 disjoint(off, len1.zextOrTrunc(bits_size_t), off2,
                          len2.zextOrTrunc(bits_size_t)));
}

expr Pointer::isBlockAlive() const {
  // NULL block is dead
  if (has_null_block && getBid().isZero())
    return false;

  auto bid = getShortBid();
  return mkIf_fold(isLocal(), m.isBlockAlive(bid, true),
                   m.isBlockAlive(bid, false));
}

expr Pointer::getAllocType() const {
  return getValue("blk_kind", m.local_blk_kind, m.non_local_blk_kind,
                   expr::mkUInt(0, 2));
}

expr Pointer::isStackAllocated() const {
  // 1) if a stack object is returned by a callee it's UB
  // 2) if a stack object is given as argument by the caller, we can upgrade it
  //    to a global object, so we can do POR here.
  if (!has_alloca || isLocal().isFalse())
    return false;
  return getAllocType() == STACK;
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

  return expr::mkIf(isLocal(), move(local), *this == other) &&
         isBlockAlive().implies(other.isBlockAlive());
}

expr Pointer::fninputRefined(const Pointer &other, set<expr> &undef,
                             bool is_byval_arg) const {
  expr size = blockSize();
  expr off = getOffsetSizet();
  expr size2 = other.blockSize();
  expr off2 = other.getOffsetSizet();

  expr local
    = expr::mkIf(isHeapAllocated(),
                 getAllocType() == other.getAllocType() &&
                 off == off2 && size == size2,

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

  return expr::mkIf(isLocal(), local, *this == other) &&
         isBlockAlive().implies(other.isBlockAlive());
}

expr Pointer::isWritable() const {
  auto bid = getShortBid();
  expr non_local
    = num_consts_src == 1 ? bid != has_null_block :
        (num_consts_src ? bid.ugt(has_null_block + num_consts_src - 1) : true);
  if (m.numNonlocals() > num_nonlocals_src + num_extra_nonconst_tgt)
    non_local &= bid.ult(num_nonlocals_src + num_extra_nonconst_tgt);
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

expr Pointer::isNocapture(bool simplify) const {
  if (!has_nocapture)
    return false;

  // local pointers can't be no-capture
  if (isLocal(simplify).isTrue())
    return false;

  return p.extract(0, 0) == 1;
}

expr Pointer::isNoRead() const {
  if (!has_noread)
    return false;
  return p.extract(has_nocapture, has_nocapture) == 1;
}

expr Pointer::isNoWrite() const {
  if (!has_nowrite)
    return false;
  unsigned idx = (unsigned)has_nocapture + (unsigned)has_noread;
  return p.extract(idx, idx) == 1;
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
  if (Memory::observesAddresses())
    return getAddress() != 0;
  return !isNull();
}

void Pointer::resetGlobals() {
  ptr_next_idx = 0;
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

}
