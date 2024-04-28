// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/pointer.h"
#include "ir/function.h"
#include "ir/memory.h"
#include "ir/globals.h"
#include "ir/state.h"
#include "util/compiler.h"

using namespace IR;
using namespace smt;
using namespace std;
using namespace util;

static expr prepend_if(const expr &pre, expr &&e, bool prepend) {
  return prepend ? pre.concat(e) : std::move(e);
}

static string local_name(const State *s, const char *name) {
  return string(name) + (s->isSource() ? "_src" : "_tgt");
}

// Assumes that both begin + len don't overflow
static expr disjoint(const expr &begin1, const expr &len1, const expr &begin2,
                     const expr &len2) {
  return begin1.uge(begin2 + len2) || begin2.uge(begin1 + len1);
}

static unsigned total_bits_short() {
  return Pointer::bitsShortBid() + Pointer::bitsShortOffset();
}

static expr attr_to_bitvec(const ParamAttrs &attrs) {
  if (!bits_for_ptrattrs)
    return {};

  uint64_t bits = 0;
  auto idx = 0;
  auto add = [&](bool b, const ParamAttrs::Attribute &a) {
    bits |= b ? (attrs.has(a) << idx++) : 0;
  };
  add(has_nocapture, ParamAttrs::NoCapture);
  add(has_noread, ParamAttrs::NoRead);
  add(has_nowrite, ParamAttrs::NoWrite);
  add(has_ptr_arg, ParamAttrs::IsArg);
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
  unsigned bits = total_bits_short() + !align * zeroBitsShortOffset();
  p = prepend_if(local.toBVBool(),
                 expr::mkVar(var_name, bits, unique_name), hasLocalBit());
  if (align)
    p = p.concat_zeros(zeroBitsShortOffset());
  if (bits_for_ptrattrs)
    p = p.concat(attr_to_bitvec(attr));
  assert(!local.isValid() || p.bits() == totalBits());
}

Pointer::Pointer(const Memory &m, expr repr) : m(m), p(std::move(repr)) {
  assert(!p.isValid() || p.bits() == totalBits());
}

Pointer::Pointer(const Memory &m, unsigned bid, bool local)
  : m(m), p(
    prepend_if(expr::mkUInt(local, 1),
               expr::mkUInt(bid, bitsShortBid())
                 .concat_zeros(bits_for_offset + bits_for_ptrattrs),
               hasLocalBit())) {
  assert((local && bid < m.numLocals()) || (!local && bid < num_nonlocals));
  assert(p.bits() == totalBits());
}

Pointer::Pointer(const Memory &m, const expr &bid, const expr &offset,
                 const ParamAttrs &attr)
  : Pointer(m, bid, offset, attr_to_bitvec(attr)) {}

expr Pointer::mkLongBid(const expr &short_bid, bool local) {
  assert((local  && (num_locals_src || num_locals_tgt)) ||
         (!local && num_nonlocals));
  if (!hasLocalBit()) {
    return short_bid;
  }
  return expr::mkUInt(local, 1).concat(short_bid);
}

expr Pointer::mkUndef(State &s) {
  auto &m = s.getMemory();
  bool force_local = false, force_nonlocal = false;
  if (hasLocalBit()) {
    force_nonlocal = m.numLocals() == 0;
    force_local    = !force_nonlocal && m.numNonlocals() == 0;
  }

  unsigned var_bits = bits_for_bid + bits_for_offset
                        - (force_local | force_nonlocal);
  expr var = expr::mkFreshVar("undef", expr::mkUInt(0, var_bits));
  s.addUndefVar(expr(var));

  if (force_local || force_nonlocal)
    var = mkLongBid(var, force_local);

  return var.concat_zeros(bits_for_ptrattrs);
}

unsigned Pointer::totalBits() {
  return bits_for_ptrattrs + bits_for_bid + bits_for_offset;
}

unsigned Pointer::bitsShortBid() {
  return bits_for_bid - hasLocalBit();
}

unsigned Pointer::bitsShortOffset() {
  return bits_for_offset - zeroBitsShortOffset();
}

unsigned Pointer::zeroBitsShortOffset() {
  assert(is_power2(bits_byte));
  return ilog2(bits_byte / 8);
}

bool Pointer::hasLocalBit() {
  return (num_locals_src || num_locals_tgt) && num_nonlocals;
}

expr Pointer::isLocal(bool simplify) const {
  if (m.numLocals() == 0)
    return false;
  if (m.numNonlocals() == 0)
    return true;

  auto bit = totalBits() - 1;
  expr local = p.extract(bit, bit);

  if (simplify) {
    switch (m.isInitialMemBlock(local, true)) {
      case 0:  break;
      case 1:  return false;
      case 2:
        // If no local escaped, pointers written to memory by a callee can't
        // alias with a local pointer.
        if (!m.hasEscapedLocals())
          return false;
        break;
      default: UNREACHABLE();
    }
  }

  return local == 1;
}

expr Pointer::isConstGlobal() const {
  auto bid = getShortBid();
  auto generic = bid.uge(has_null_block) &&
                 expr(num_consts_src > 0) &&
                 bid.ule(num_consts_src + has_null_block - 1);
  auto tgt
    = (num_nonlocals_src == 0 ? expr(true) : bid.ugt(num_nonlocals_src-1)) &&
      expr(num_nonlocals != num_nonlocals_src);
  return !isLocal() && (generic || tgt);
}

expr Pointer::isWritableGlobal() const {
  auto bid = getShortBid();
  return !isLocal() &&
         bid.uge(has_null_block + num_consts_src) &&
         expr(num_globals_src > 0) &&
         bid.ule(has_null_block + num_globals_src - 1);
}

expr Pointer::getBid() const {
  return p.extract(totalBits() - 1, bits_for_offset + bits_for_ptrattrs);
}

expr Pointer::getShortBid() const {
  return p.extract(totalBits() - 1 - hasLocalBit(),
                   bits_for_offset + bits_for_ptrattrs);
}

expr Pointer::getOffset() const {
  return p.extract(bits_for_offset + bits_for_ptrattrs - 1, bits_for_ptrattrs);
}

expr Pointer::getOffsetSizet() const {
  auto off = getOffset();
  return bits_for_offset >= bits_size_t ? off : off.sextOrTrunc(bits_size_t);
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

expr Pointer::getBlockBaseAddress(bool simplify) const {
  assert(Memory::observesAddresses());

  auto bid = getShortBid();
  auto zero = expr::mkUInt(0, bits_ptr_address - hasLocalBit());
  // fast path for null ptrs
  auto non_local
    = simplify && bid.isZero() && has_null_block ?
          zero : expr::mkUF("blk_addr", { bid }, zero);
  // Non-local block area is the lower half
  if (hasLocalBit())
    non_local = expr::mkUInt(0, 1).concat(non_local);

  if (auto local = m.local_blk_addr(bid)) {
    // Local block area is the upper half of the memory
    expr lc = hasLocalBit() ? expr::mkUInt(1, 1).concat(*local) : *local;
    return expr::mkIf(isLocal(), lc, non_local);
  } else
    return non_local;
}

expr Pointer::getAddress(bool simplify) const {
  return
    getBlockBaseAddress(simplify) + getOffset().zextOrTrunc(bits_ptr_address);
}

expr Pointer::blockSize() const {
  return getValue("blk_size", m.local_blk_size, m.non_local_blk_size,
                  expr::mkUInt(0, bits_size_t));
}

expr Pointer::blockSizeOffsetT() const {
  expr sz = blockSize();
  return bits_for_offset > bits_size_t ? sz.zextOrTrunc(bits_for_offset) : sz;
}

expr Pointer::reprWithoutAttrs() const {
  return p.extract(totalBits() - 1, bits_for_ptrattrs);
}

Pointer Pointer::mkPointerFromNoAttrs(const Memory &m, const expr &e) {
  return { m, e.concat_zeros(bits_for_ptrattrs) };
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

Pointer Pointer::maskOffset(const expr &mask) const {
  return { m, getBid(),
           ((getAddress() & mask.zextOrTrunc(bits_ptr_address))
               - getBlockBaseAddress()).zextOrTrunc(bits_for_offset),
           getAttrs() };
}

expr Pointer::addNoOverflow(const expr &offset) const {
  return getOffset().add_no_soverflow(offset);
}

expr Pointer::operator==(const Pointer &rhs) const {
  return reprWithoutAttrs() == rhs.reprWithoutAttrs();
}

expr Pointer::operator!=(const Pointer &rhs) const {
  return !operator==(rhs);
}

static expr inbounds(const Pointer &p, bool strict) {
  if (isUndef(p.getOffset()))
    return false;

  auto offset = p.getOffsetSizet();
  auto size   = p.blockSizeOffsetT();
  return (strict ? offset.ult(size) : offset.ule(size)) && !offset.isNegative();
}

expr Pointer::inbounds(bool simplify_ptr, bool strict) {
  if (!simplify_ptr)
    return ::inbounds(*this, strict);

  DisjointExpr<expr> ret(expr(false)), all_ptrs;
  for (auto &[ptr_expr, domain] : DisjointExpr<expr>(p, 3)) {
    expr inb = ::inbounds(Pointer(m, ptr_expr), strict);
    if (!inb.isFalse())
      all_ptrs.add(ptr_expr, domain);
    ret.add(std::move(inb), domain);
  }

  // trim set of valid ptrs
  if (auto ptrs = std::move(all_ptrs)())
    p = *ptrs;
  else
    p = expr::mkUInt(0, totalBits());

  return *std::move(ret)();
}

expr Pointer::blockAlignment() const {
  return getValue("blk_align", m.local_blk_align, m.non_local_blk_align,
                   expr::mkUInt(0, 6), true);
}

expr Pointer::isBlockAligned(uint64_t align, bool exact) const {
  if (!exact && align == 1)
    return true;

  auto bits = ilog2(align);
  expr blk_align = blockAlignment();
  return exact ? blk_align == bits : blk_align.uge(bits);
}

expr Pointer::isAligned(uint64_t align) {
  if (align == 1)
    return true;
  if (!is_power2(align))
    return isNull();

  auto offset = getOffset();
  if (isUndef(offset))
    return false;

  auto bits = min(ilog2(align), bits_for_offset);

  expr blk_align = isBlockAligned(align);

  // TODO: allow this in more cases. for example when the block is local
  // and addresses are not observed.
  if (blk_align.isConst() && offset.isConst()) {
    // This is stricter than checking getAddress(), but as addresses are not
    // observed, program shouldn't be able to distinguish this from checking
    // getAddress()
    auto zero = expr::mkUInt(0, bits);
    auto newp = p.extract(totalBits() - 1, bits_for_ptrattrs + bits)
                 .concat(zero);
    if (bits_for_ptrattrs)
      newp = newp.concat(getAttrs());
    p = std::move(newp);
    assert(!p.isValid() || p.bits() == totalBits());
    return { blk_align && offset.extract(bits - 1, 0) == zero };
  }

  return getAddress().extract(bits - 1, 0) == 0;
}

expr Pointer::isAligned(const expr &align) {
  uint64_t n;
  if (align.isUInt(n))
    return isAligned(n);

  return
    expr::mkIf(align.isPowerOf2(),
               getAddress().urem(align.zextOrTrunc(bits_ptr_address)) == 0,
               isNull());
}

static pair<expr, expr> is_dereferenceable(Pointer &p,
                                           const expr &bytes_off,
                                           const expr &bytes,
                                           uint64_t align, bool iswrite,
                                           bool ignore_accessability) {
  expr block_sz = p.blockSizeOffsetT();
  expr offset = p.getOffset();

  // check that offset is within bounds and that arith doesn't overflow
  expr cond = (offset + bytes_off).sextOrTrunc(block_sz.bits()).ule(block_sz);
  cond &= !offset.isNegative();
  if (!block_sz.isNegative().isFalse()) // implied if block_sz >= 0
    cond &= offset.add_no_soverflow(bytes_off);

  cond &= p.isBlockAlive();

  if (!ignore_accessability) {
    if (iswrite)
      cond &= p.isWritable() && !p.isNoWrite();
    else
      cond &= !p.isNoRead();
  }

  // try some constant folding; these are implied by the conditions above
  if (bytes.ugt(p.blockSize()).isTrue() ||
      p.getOffsetSizet().uge(block_sz).isTrue() ||
      isUndef(offset))
    cond = false;

  return { std::move(cond), p.isAligned(align) };
}

// When bytes is 0, pointer is always dereferenceable
pair<AndExpr, expr>
Pointer::isDereferenceable(const expr &bytes0, uint64_t align,
                           bool iswrite, bool ignore_accessability) {
  expr bytes = bytes0.zextOrTrunc(bits_size_t)
                     .round_up(expr::mkUInt(align, bits_size_t));
  expr bytes_off = bytes.zextOrTrunc(bits_for_offset);

  DisjointExpr<expr> UB(expr(false)), is_aligned(expr(false)), all_ptrs;

  for (auto &[ptr_expr, domain] : DisjointExpr<expr>(p, 3)) {
    Pointer ptr(m, ptr_expr);
    auto [ub, aligned] = ::is_dereferenceable(ptr, bytes_off, bytes, align,
                                              iswrite, ignore_accessability);

    // record pointer if not definitely unfeasible
    if (!ub.isFalse() && !aligned.isFalse() && !ptr.blockSize().isZero())
      all_ptrs.add(std::move(ptr).release(), domain);

    UB.add(std::move(ub), domain);
    is_aligned.add(std::move(aligned), domain);
  }

  AndExpr exprs;
  exprs.add(bytes == 0 || *std::move(UB)());

  // cannot store more bytes than address space
  if (bytes0.bits() > bits_size_t)
    exprs.add(bytes0.extract(bytes0.bits() - 1, bits_size_t) == 0);

  // address must be always aligned regardless of access size
  // exprs.add(*std::move(is_aligned)());

  // trim set of valid ptrs
  auto ptrs = std::move(all_ptrs)();
  p = ptrs ? *std::move(ptrs) : expr::mkUInt(0, totalBits());

  return { std::move(exprs), *std::move(is_aligned)() };
}

pair<AndExpr, expr>
Pointer::isDereferenceable(uint64_t bytes, uint64_t align,
                           bool iswrite, bool ignore_accessability) {
  return isDereferenceable(expr::mkUInt(bytes, bits_size_t), align, iswrite,
                           ignore_accessability);
}

// This function assumes that both begin + len don't overflow
void Pointer::isDisjointOrEqual(const expr &len1, const Pointer &ptr2,
                                const expr &len2) const {
  auto off = getOffsetSizet();
  auto off2 = ptr2.getOffsetSizet();
  unsigned bits = off.bits();
  m.state->addUB(getBid() != ptr2.getBid() ||
                 off == off2 ||
                 disjoint(off, len1.zextOrTrunc(bits), off2,
                          len2.zextOrTrunc(bits)));
}

expr Pointer::isBlockAlive() const {
  // NULL block is dead
  if (has_null_block && !null_is_dereferenceable && getBid().isZero())
    return false;

  auto bid = getShortBid();
  return mkIf_fold(isLocal(), m.isBlockAlive(bid, true),
                   m.isBlockAlive(bid, false));
}

expr Pointer::getAllocType() const {
  return getValue("blk_kind", m.local_blk_kind, m.non_local_blk_kind,
                   expr::mkUInt(0, 2));
}

expr Pointer::isStackAllocated(bool simplify) const {
  // 1) if a stack object is returned by a callee it's UB
  // 2) if a stack object is given as argument by the caller, we can upgrade it
  //    to a global object, so we can do POR here.
  if (simplify && (!has_alloca || isLocal().isFalse()))
    return false;
  return getAllocType() == STACK;
}

expr Pointer::isHeapAllocated() const {
  assert(MALLOC == 2 && CXX_NEW == 3);
  return getAllocType().extract(1, 1) == 1;
}

expr Pointer::refined(const Pointer &other) const {
  bool is_asm = other.m.isAsmMode();

  // This refers to a block that was malloc'ed within the function
  expr local = other.isLocal();
  local &= getAllocType() == other.getAllocType();
  local &= blockSize() == other.blockSize();
  local &= getOffset() == other.getOffset();
  // Attributes are ignored at refinement.

  // TODO: this induces an infinite loop
  //local &= block_refined(other);

  expr nonlocal = is_asm ? getAddress() == other.getAddress() : *this == other;

  Pointer other_deref
    = is_asm ? other.m.searchPointer(other.getAddress()) : other;

  return expr::mkIf(isNull(), other.isNull(),
                    expr::mkIf(isLocal(), std::move(local), nonlocal) &&
                      // FIXME: this should be disabled just for phy pointers
                      (is_asm ? expr(true)
                        : isBlockAlive().implies(other_deref.isBlockAlive())));
}

expr Pointer::fninputRefined(const Pointer &other, set<expr> &undef,
                             const expr &byval_bytes) const {
  bool is_asm = other.m.isAsmMode();
  expr size = blockSizeOffsetT();
  expr off = getOffsetSizet();
  expr size2 = other.blockSizeOffsetT();
  expr off2 = other.getOffsetSizet();

  // TODO: check block value for byval_bytes
  if (!byval_bytes.isZero())
    return true;

  expr local
    = expr::mkIf(isHeapAllocated(),
                 getAllocType() == other.getAllocType() &&
                   off == off2 && size == size2,

                 expr::mkIf(off.sge(0),
                            off2.sge(0) &&
                              expr::mkIf(off.ule(size),
                                         off2.ule(size2) && off2.uge(off) &&
                                           (size2 - off2).uge(size - off),
                                         off2.ugt(size2) && off == off2 &&
                                           size2.uge(size)),
                            // maintains same dereferenceability before/after
                            off == off2 && size2.uge(size)));
  local = (other.isLocal() || other.isByval()) && local;

  // TODO: this induces an infinite loop
  // block_refined(other);

  expr nonlocal = is_asm ? getAddress() == other.getAddress() : *this == other;

  Pointer other_deref
    = is_asm ? other.m.searchPointer(other.getAddress()) : other;

  return expr::mkIf(isNull(), other.isNull(),
                    expr::mkIf(isLocal(), local, nonlocal) &&
                      // FIXME: this should be disabled just for phy pointers
                      (is_asm ? expr(true)
                        : isBlockAlive().implies(other_deref.isBlockAlive())));
}

expr Pointer::isWritable() const {
  auto bid = getShortBid();
  expr non_local
    = num_consts_src == 1 ? bid != has_null_block :
        (num_consts_src ? bid.ugt(has_null_block + num_consts_src - 1) : true);
  if (m.numNonlocals() > num_nonlocals_src)
    non_local &= bid.ult(num_nonlocals_src);
  if (has_null_block && null_is_dereferenceable)
    non_local |= bid == 0;

  // check for non-writable byval blocks (which are non-local)
  for (auto [byval, is_const] : m.byval_blks) {
    if (is_const)
      non_local &= bid != byval;
  }

  return isLocal() || non_local;
}

expr Pointer::isByval() const {
  auto this_bid = getShortBid();
  expr non_local(false);
  for (auto [bid, is_const] : m.byval_blks) {
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

#define GET_ATTR(attr, idx)          \
  if (!attr)                         \
    return false;                    \
  unsigned idx_ = idx;               \
  return p.extract(idx_, idx_) == 1;

expr Pointer::isNoRead() const {
  GET_ATTR(has_noread, (unsigned)has_nocapture);
}

expr Pointer::isNoWrite() const {
  GET_ATTR(has_nowrite, (unsigned)has_nocapture + (unsigned)has_noread);
}

expr Pointer::isBasedOnArg() const {
  GET_ATTR(has_ptr_arg, (unsigned)has_nocapture + (unsigned)has_noread +
                        (unsigned)has_nowrite);
}

Pointer Pointer::setAttrs(const ParamAttrs &attr) const {
  return { m, getBid(), getOffset(), getAttrs() | attr_to_bitvec(attr) };
}

Pointer Pointer::setIsBasedOnArg() const {
  unsigned idx = (unsigned)has_nocapture + (unsigned)has_noread +
                 (unsigned)has_nowrite;
  auto attrs = getAttrs();
  return { m, getBid(), getOffset(), attrs | expr::mkUInt(1 << idx, attrs) };
}

Pointer Pointer::mkNullPointer(const Memory &m) {
  assert(has_null_block);
  // A null pointer points to block 0 without any attribute.
  return { m, 0, false };
}

expr Pointer::isNull() const {
  if (Memory::observesAddresses())
    return getAddress() == 0;
  if (!has_null_block)
    return false;
  return *this == mkNullPointer(m);
}

Pointer
Pointer::mkIf(const expr &cond, const Pointer &then, const Pointer &els) {
  assert(&then.m == &els.m);
  return Pointer(then.m, expr::mkIf(cond, then.p, els.p));
}

ostream& operator<<(ostream &os, const Pointer &p) {
  if (p.isNull().isTrue())
    return os << "null";

#define P(field, fn)   \
  if (field.isConst()) \
    field.fn(os);      \
  else                 \
    os << field

  os << "pointer(";
  if (p.isLocal().isConst())
    os << (p.isLocal().isTrue() ? "local" : "non-local");
  else
    os << "local=" << p.isLocal();
  os << ", block_id=";
  P(p.getShortBid(), printUnsigned);

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
