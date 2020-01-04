// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/memory.h"
#include "ir/globals.h"
#include "ir/state.h"
#include "ir/value.h"
#include "util/compiler.h"
#include <string>

using namespace IR;
using namespace smt;
using namespace std;
using namespace util;

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
  assert(bits_byte <= bits_program_pointer);
  return bits_byte != bits_program_pointer ? 3 : 0;
}

static unsigned padding_ptr_byte() {
  return Byte::bitsByte() - does_int_mem_access - 1 - Pointer::total_bits()
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

expr Byte::is_ptr() const {
  if (!does_ptr_mem_access)
    return false;
  if (!does_int_mem_access)
    return true;
  auto bit = p.bits() - 1;
  return p.extract(bit, bit) == 1;
}

expr Byte::ptr_nonpoison() const {
  if (!does_ptr_mem_access)
    return true;
  auto bit = p.bits() - 1 - byte_has_ptr_bit();
  return p.extract(bit, bit) == 0;
}

Pointer Byte::ptr() const {
  return { m, ptr_value() };
}

expr Byte::ptr_value() const {
  if (!does_ptr_mem_access)
    return expr::mkUInt(0, Pointer::total_bits());

  auto start = bits_ptr_byte_offset() + padding_ptr_byte();
  return p.extract(Pointer::total_bits() + start - 1, start);
}

expr Byte::ptr_byteoffset() const {
  if (!does_ptr_mem_access)
    return expr::mkUInt(0, bits_ptr_byte_offset());
  if (bits_ptr_byte_offset() == 0)
    return expr::mkUInt(0, 1);

  unsigned start = padding_ptr_byte();
  return p.extract(bits_ptr_byte_offset() + start - 1, start);
}

expr Byte::nonptr_nonpoison() const {
  if (!does_int_mem_access)
    return expr::mkUInt(0, bits_int_poison());
  unsigned start = padding_nonptr_byte() + bits_byte;
  return p.extract(start + bits_int_poison() - 1, start);
}

expr Byte::nonptr_value() const {
  if (!does_int_mem_access)
    return expr::mkUInt(0, bits_byte);
  unsigned start = padding_nonptr_byte();
  return p.extract(start + bits_byte - 1, start);
}

expr Byte::is_poison(bool fullbit) const {
  expr np = nonptr_nonpoison();
  if (byte_has_ptr_bit() && bits_int_poison() == 1) {
    assert(!np.isValid() || ptr_nonpoison().eq(np == 0));
    return np == 1;
  }
  return expr::mkIf(is_ptr(), !ptr_nonpoison(),
                              fullbit ? np == -1ull : np != 0);
}

expr Byte::is_zero() const {
  return expr::mkIf(is_ptr(), ptr().isNull(), nonptr_value() == 0);
}

unsigned Byte::bitsByte() {
  unsigned ptr_bits = does_ptr_mem_access *
                        (1 + Pointer::total_bits() + bits_ptr_byte_offset());
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
  if (byte.is_poison().isTrue())
    return os << "poison";

  if (byte.is_ptr().isTrue()) {
    os << byte.ptr() << ", byte offset=";
    byte.ptr_byteoffset().printSigned(os);
  } else {
    auto np = byte.nonptr_nonpoison();
    auto val = byte.nonptr_value();
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
    auto nullp = Pointer::mkNullPointer(m);
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
      expr ptr_value = b.ptr_value();
      expr b_is_ptr  = b.is_ptr();

      if (i == 0) {
        loaded_ptr = ptr_value;
        is_ptr     = move(b_is_ptr);
      } else {
        non_poison &= is_ptr == b_is_ptr;
      }
      non_poison &=
        expr::mkIf(is_ptr,
                   b.ptr_byteoffset() == i && ptr_value == loaded_ptr,
                   b.nonptr_value() == 0);
      non_poison &= !b.is_poison(false);
    }
    return { expr::mkIf(is_ptr, loaded_ptr, nullp()), move(non_poison) };

  } else {
    auto bitsize = toType.bits();
    assert(divide_up(bitsize, bits_byte) == bytes.size());

    StateValue val;
    bool first = true;
    IntType ibyteTy("", bits_byte);

    for (auto &b: bytes) {
      StateValue v(b.nonptr_value(),
                   ibyteTy.combine_poison(!b.is_ptr(), b.nonptr_nonpoison()));
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
  return num_locals && num_nonlocals;
}

static unsigned bits_shortbid() {
  return bits_for_bid - ptr_has_local_bit();
}

static expr attr_to_bitvec(unsigned attributes) {
  if (!bits_for_ptrattrs)
    return expr();

  uint64_t bits = 0;
  auto idx = 0;
  auto to_bit = [&](bool b, Input::Attribute a) -> uint64_t {
    return b ? (((attributes & a) ? 1 : 0) << idx++) : 0;
  };
  bits |= to_bit(has_nocapture, Input::NoCapture);
  bits |= to_bit(has_readonly, Input::ReadOnly);
  bits |= to_bit(has_readnone, Input::ReadNone);
  return expr::mkUInt(bits, bits_for_ptrattrs);
}

namespace IR {

Pointer::Pointer(const Memory &m, const char *var_name, const expr &local,
                 bool unique_name, const expr &attr) : m(m) {
  string name = var_name;
  if (unique_name)
    name += '!' + to_string(ptr_next_idx++);

  p = prepend_if(local.toBVBool(),
                 expr::mkVar(name.c_str(), total_bits_short()),
                 ptr_has_local_bit()).concat_zeros(zero_bits_offset());
  if (bits_for_ptrattrs)
    p = p.concat(attr.isValid() ? attr : expr::mkUInt(0, bits_for_ptrattrs));
  assert(!local.isValid() || p.bits() == total_bits());
}

Pointer::Pointer(const Memory &m, expr repr) : m(m), p(move(repr)) {
  assert(!p.isValid() || p.bits() == total_bits());
}

Pointer::Pointer(const Memory &m, unsigned bid, bool local)
  : m(m), p(
    prepend_if(expr::mkUInt(local, 1),
               expr::mkUInt(bid, bits_shortbid())
                 .concat_zeros(bits_for_offset + bits_for_ptrattrs),
               ptr_has_local_bit())) {
  assert((local && bid < num_locals) || (!local && bid < num_nonlocals));
  assert(p.bits() == total_bits());
}

Pointer::Pointer(const Memory &m, const expr &bid, const expr &offset,
                 const expr &attr) : m(m), p(bid.concat(offset)) {
  if (bits_for_ptrattrs)
    p = p.concat(attr.isValid() ? attr : expr::mkUInt(0, bits_for_ptrattrs));
  assert(!bid.isValid() || !offset.isValid() || p.bits() == total_bits());
}

unsigned Pointer::total_bits() {
  return bits_for_ptrattrs + bits_for_bid + bits_for_offset;
}

unsigned Pointer::total_bits_short() {
  return bits_shortbid() + bits_for_offset - zero_bits_offset();
}

expr Pointer::is_local() const {
  if (num_locals == 0)
    return false;
  if (m.num_nonlocals() == 0)
    return true;
  auto bit = total_bits() - 1;
  return p.extract(bit, bit) == 1;
}

expr Pointer::get_bid() const {
  return p.extract(total_bits() - 1, bits_for_offset + bits_for_ptrattrs);
}

expr Pointer::get_short_bid() const {
  return p.extract(total_bits() - 1 - ptr_has_local_bit(),
                   bits_for_offset + bits_for_ptrattrs);
}

expr Pointer::get_offset() const {
  return p.extract(bits_for_offset + bits_for_ptrattrs - 1, bits_for_ptrattrs);
}

expr Pointer::get_offset_sizet() const {
  return get_offset().sextOrTrunc(bits_size_t);
}

expr Pointer::get_attrs() const {
  return bits_for_ptrattrs ? p.extract(bits_for_ptrattrs - 1, 0) : expr();
}

expr Pointer::get_value(const char *name, const FunctionExpr &local_fn,
                        const FunctionExpr &nonlocal_fn,
                        const expr &ret_type, bool src_name) const {
  if (!p.isValid())
    return {};

  auto bid = get_short_bid();
  expr non_local;

  if (auto val = nonlocal_fn.lookup(bid))
    non_local = *val;
  else {
    string uf = src_name ? local_name(m.state, name) : name;
    non_local = expr::mkUF(uf.c_str(), { bid }, ret_type);
  }

  if (auto local = local_fn(bid))
    return expr::mkIf(is_local(), *local, non_local);
  return non_local;
}

expr Pointer::get_address(bool simplify) const {
  assert(observes_addresses());

  auto bid = get_short_bid();
  auto zero = expr::mkUInt(0, bits_size_t - 1);
  // fast path for null ptrs
  auto non_local
    = simplify && bid.isZero() ? zero : expr::mkUF("blk_addr", { bid }, zero);
  // Non-local block area is the lower half
  non_local = expr::mkUInt(0, 1).concat(non_local);

  expr addr;
  if (auto local = m.local_blk_addr(bid))
    // Local block area is the upper half of the memory
    addr = expr::mkIf(is_local(), expr::mkUInt(1, 1).concat(*local), non_local);
  else
    addr = move(non_local);

  return addr + get_offset_sizet();
}

expr Pointer::block_size() const {
  // ASSUMPTION: programs can only allocate up to half of address space
  // so the first bit of size is always zero.
  // We need this assumption to support negative offsets.
  return expr::mkUInt(0, 1)
           .concat(get_value("blk_size", m.local_blk_size, m.non_local_blk_size,
                             expr::mkUInt(0, bits_size_t - 1)));
}

expr Pointer::short_ptr() const {
  auto off = zero_bits_offset();
  return p.extract(total_bits() - 1 - ptr_has_local_bit(),
                   bits_for_ptrattrs + off);
}

Pointer Pointer::operator+(const expr &bytes) const {
  return { m, get_bid(), get_offset() + bytes.zextOrTrunc(bits_for_offset),
           get_attrs() };
}

Pointer Pointer::operator+(unsigned bytes) const {
  return *this + expr::mkUInt(bytes, bits_for_offset);
}

void Pointer::operator+=(const expr &bytes) {
  p = (*this + bytes).p;
}

expr Pointer::add_no_overflow(const expr &offset) const {
  return get_offset().add_no_soverflow(offset);
}

expr Pointer::operator==(const Pointer &rhs) const {
  return p.extract(total_bits() - 1, bits_for_ptrattrs) ==
         rhs.p.extract(total_bits() - 1, bits_for_ptrattrs);
}

expr Pointer::operator!=(const Pointer &rhs) const {
  return !operator==(rhs);
}

#define DEFINE_CMP(op)                                                      \
StateValue Pointer::op(const Pointer &rhs) const {                          \
  /* Note that attrs are not compared. */                                   \
  return { get_offset().op(rhs.get_offset()), get_bid() == rhs.get_bid() }; \
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
  if (isUndef(p.get_offset()))
    return false;

  // equivalent to offset >= 0 && offset <= block_size
  // because block_size u<= 0x7FFF..FF
  auto ofs = p.get_offset_sizet();
  return strict ? ofs.ult(p.block_size()) : ofs.ule(p.block_size());
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
    p = expr::mkUInt(0, total_bits());

  return *ret();
}

expr Pointer::block_alignment() const {
  return get_value("blk_align", m.local_blk_align, m.non_local_blk_align,
                   expr::mkUInt(0, 8), true);
}

expr Pointer::is_block_aligned(unsigned align, bool exact) const {
  assert(align >= bits_byte / 8);
  if (!exact && align == 1)
    return true;

  auto bits = ilog2(align);
  expr blk_align = block_alignment();
  return exact ? blk_align == bits : blk_align.uge(bits);
}

expr Pointer::is_aligned(unsigned align) const {
  assert(align >= bits_byte / 8);
  if (align == 1)
    return true;

  auto bits = min(ilog2(align), bits_for_offset);

  if (!observes_addresses())
    // This is stricter than checking get_address(), but as addresses are not
    // observed, program shouldn't be able to distinguish this from checking
    // get_address()
    return is_block_aligned(align, false) &&
           get_offset().extract(bits - 1, 0) == 0;

  return get_address().extract(bits - 1, 0) == 0;
}

static pair<expr, expr> is_dereferenceable(const Pointer &p,
                                           const expr &bytes_off,
                                           const expr &bytes,
                                           unsigned align, bool iswrite) {
  expr block_sz = p.block_size();
  expr offset = p.get_offset();

  // check that offset is within bounds and that arith doesn't overflow
  expr cond = (offset + bytes_off).sextOrTrunc(bits_size_t).ule(block_sz);
  cond &= offset.add_no_uoverflow(bytes_off);

  cond &= p.is_block_alive();
  cond &= !p.is_readnone();

  if (iswrite)
    cond &= p.is_writable() && !p.is_readonly();

  // try some constant folding; these are implied by the conditions above
  if (bytes.ugt(block_sz).isTrue() ||
      offset.sextOrTrunc(bits_size_t).uge(block_sz).isTrue() ||
      isUndef(offset))
    cond = false;

  return { move(cond), p.is_aligned(align) };
}

// When bytes is 0, pointer is always derefenceable
void Pointer::is_dereferenceable(const expr &bytes0, unsigned align,
                                 bool iswrite) {
  expr bytes_off = bytes0.zextOrTrunc(bits_for_offset);
  expr bytes = bytes0.zextOrTrunc(bits_size_t);
  DisjointExpr<expr> UB(expr(false)), is_aligned(expr(false)), all_ptrs;

  for (auto &[ptr_expr, domain] : DisjointExpr<expr>(p, true, true)) {
    Pointer ptr(m, ptr_expr);
    auto [ub, aligned] = ::is_dereferenceable(ptr, bytes_off, bytes, align,
                                              iswrite);

    // record pointer if not definitely unfeasible
    if (!ub.isFalse() && !aligned.isFalse() && !ptr.block_size().isZero())
      all_ptrs.add(ptr_expr, domain);

    UB.add(move(ub), domain);
    is_aligned.add(move(aligned), domain);
  }

  m.state->addUB(bytes == 0 || *UB());

  // cannot store more bytes than address space
  if (bytes0.bits() > bits_size_t)
    m.state->addUB(bytes0.extract(bytes0.bits() - 1, bits_size_t) == 0);

  // address must be always aligned regardless of access size
  m.state->addUB(*is_aligned());

  // trim set of valid ptrs
  if (auto ptrs = all_ptrs())
    p = *ptrs;
  else
    p = expr::mkUInt(0, total_bits());
}

void Pointer::is_dereferenceable(unsigned bytes, unsigned align, bool iswrite) {
  is_dereferenceable(expr::mkUInt(bytes, bits_size_t), align, iswrite);
}

// general disjoint check for unsigned integer
// This function assumes that both begin + len don't overflow
static expr disjoint(const expr &begin1, const expr &len1, const expr &begin2,
                     const expr &len2) {
  return begin1.uge(begin2 + len2) || begin2.uge(begin1 + len1);
}

// This function assumes that both begin + len don't overflow
void Pointer::is_disjoint(const expr &len1, const Pointer &ptr2,
                           const expr &len2) const {
  m.state->addUB(get_bid() != ptr2.get_bid() ||
                  disjoint(get_offset_sizet(),
                           len1.zextOrTrunc(bits_size_t),
                           ptr2.get_offset_sizet(),
                           len2.zextOrTrunc(bits_size_t)));
}

expr Pointer::is_block_alive() const {
  // If programs have no free(), we assume all blocks are always live.
  // For non-local blocks, there's enough non-determinism through block size,
  // that can be 0 or non-0
  if (!has_free && !has_fncall && !has_dead_allocas)
    return true;

  auto bid = get_short_bid();
  return expr::mkIf(is_local(), m.local_block_liveness.load(bid),
                    m.non_local_block_liveness.load(bid));
}

expr Pointer::get_alloc_type() const {
  // If programs have no malloc & free, we don't need to store this information
  // since it is only used to check if free/delete is ok and
  // for memory refinement of local malloc'ed blocks
  if (!has_malloc && !has_free && !has_fncall)
    return expr::mkUInt(NON_HEAP, 2);

  // if malloc is used, but no free, we can still ignore info for non-locals
  if (!has_free) {
    FunctionExpr non_local;
    non_local.add(get_short_bid(), expr::mkUInt(NON_HEAP, 2));
    return get_value("blk_kind", m.local_blk_kind, non_local, expr());

  }
  return get_value("blk_kind", m.local_blk_kind, m.non_local_blk_kind,
                   expr::mkUInt(0, 2));
}

expr Pointer::is_heap_allocated() const {
  return get_alloc_type() != NON_HEAP;
}

expr Pointer::refined(const Pointer &other) const {
  // This refers to a block that was malloc'ed within the function
  expr local = get_alloc_type() == other.get_alloc_type();
  local &= block_size() == other.block_size();
  local &= get_offset() == other.get_offset();
  // Attributes are ignored at refinement.

  // TODO: this induces an infinite loop
  //local &= block_refined(other);

  return is_block_alive().implies(
           other.is_block_alive() &&
             expr::mkIf(is_local(), is_heap_allocated().implies(local),
                        *this == other));
}

expr Pointer::fninput_refined(const Pointer &other) const {
  expr size = block_size();
  expr off = get_offset_sizet();
  expr size2 = other.block_size();
  expr off2 = other.get_offset_sizet();

  expr local
    = expr::mkIf(is_heap_allocated(),
                 other.is_heap_allocated() && off == off2 && size2.uge(size),

                 // must maintain same dereferenceability before & after
                 expr::mkIf(off.sle(-1),
                            off == off2 && size2.uge(size),
                            off2.sge(0) &&
                              expr::mkIf(off.sle(size),
                                         off2.sle(size2) && off2.uge(off) &&
                                           (size2 - off2).uge(size - off),
                                         off2.sgt(size2) && off == off2 &&
                                           size2.uge(size))));
  local = (other.is_local() || other.is_byval()) && local;

  // TODO: this induces an infinite loop
  // block_refined(other);

  return is_block_alive().implies(
           other.is_block_alive() &&
             expr::mkIf(is_local(), local, *this == other));
}

expr Pointer::block_val_refined(const Pointer &other) const {
  Byte val(m, m.non_local_block_val.load(short_ptr()));
  Byte val2(other.m, other.m.non_local_block_val.load(other.short_ptr()));

  // refinement if offset had non-ptr value
  expr np1 = val.nonptr_nonpoison();
  expr int_cnstr = does_sub_byte_access
                     ? (val2.nonptr_nonpoison() | np1) == np1 &&
                       (val.nonptr_value() | np1) == (val2.nonptr_value() | np1)
                     : val2.nonptr_nonpoison() == 0 &&
                       val.nonptr_value() == val2.nonptr_value();

  // fast path: if we didn't do any ptr store, then all ptrs in memory were
  // already there and don't need checking
  expr is_ptr = val.is_ptr();
  expr is_ptr2 = val2.is_ptr();
  expr ptr_cnstr;
  if (!does_ptr_store || is_ptr.isFalse() || is_ptr2.isFalse()) {
    ptr_cnstr = val == val2;
  } else {
    ptr_cnstr = val2.ptr_nonpoison() &&
                val.ptr_byteoffset() == val2.ptr_byteoffset() &&
                val.ptr().refined(val2.ptr());
  }
  return val.is_poison() ||
         expr::mkIf(is_ptr == is_ptr2,
                    expr::mkIf(is_ptr, ptr_cnstr, int_cnstr),
                    // allow null ptr <-> zero
                    val.is_zero() && !val2.is_poison() && val2.is_zero());
}

expr Pointer::block_refined(const Pointer &other) const {
  expr blk_size = block_size();
  expr val_refines(true);
  uint64_t bytes;
  auto bytes_per_byte = bits_byte / 8;

  if (blk_size.isUInt(bytes) && (bytes / bytes_per_byte) <= 8) {
    expr bid = get_bid();
    expr ptr_offset = get_offset();

    for (unsigned off = 0; off < bytes; off += bytes_per_byte) {
      expr off_expr = expr::mkUInt(off, bits_for_offset);
      Pointer p(m, bid, off_expr);
      Pointer q(other.m, p());
      val_refines &= (ptr_offset == off_expr).implies(p.block_val_refined(q));
    }
  } else {
    val_refines = block_val_refined(other);
  }

  return is_block_alive() == other.is_block_alive() &&
         blk_size == other.block_size() &&
         get_alloc_type() == other.get_alloc_type() &&
         is_writable() == other.is_writable() &&
         m.state->simplifyWithAxioms(
           block_alignment().ule(other.block_alignment())) &&
         (is_block_alive() && get_offset_sizet().ult(blk_size))
           .implies(val_refines);
}

expr Pointer::is_writable() const {
  auto this_bid = get_short_bid();
  expr non_local(true);
  for (auto bid : m.non_local_blk_nonwritable) {
    non_local &= this_bid != bid;
  }
  return is_local() || non_local;
}

expr Pointer::is_byval() const {
  auto this_bid = get_short_bid();
  expr non_local(false);
  for (auto bid : m.byval_blks) {
    non_local |= this_bid == bid;
  }
  return !is_local() && non_local;
}

expr Pointer::is_nocapture() const {
  if (!has_nocapture)
    return false;

  // local pointers can't be no-capture
  if (is_local().isTrue())
    return false;

  return p.extract(0, 0) == 1;
}

expr Pointer::is_readonly() const {
  if (!has_readonly)
    return false;
  return p.extract(has_nocapture, has_nocapture) == 1;
}

expr Pointer::is_readnone() const {
  if (!has_readnone)
    return false;
  unsigned idx = (unsigned)has_nocapture + (unsigned)has_readonly;
  return p.extract(idx, idx) == 1;
}

void Pointer::strip_attrs() {
  p = p.extract(total_bits()-1, bits_for_ptrattrs)
       .concat_zeros(bits_for_ptrattrs);
}

Pointer Pointer::mkNullPointer(const Memory &m) {
  // Null pointer exists if either source or target uses it.
  assert(num_nonlocals > 0);
  // A null pointer points to block 0 without any attribute.
  return { m, 0, false };
}

expr Pointer::isNull() const {
  if (num_nonlocals == 0)
    return false;
  return *this == mkNullPointer(m);
}

expr Pointer::isNonZero() const {
  if (observes_addresses())
    return get_address() != 0;
  return !isNull();
}

ostream& operator<<(ostream &os, const Pointer &p) {
  if (p.isNull().isTrue())
    return os << "null";

#define P(field, fn)   \
  if (field.isConst()) \
    field.fn(os);      \
  else                 \
    os << field

  os << "pointer(" << (p.is_local().isTrue() ? "local" : "non-local")
     << ", block_id=";
  P(p.get_bid(), printUnsigned);

  os << ", offset=";
  P(p.get_offset(), printSigned);

  if (bits_for_ptrattrs && !p.get_attrs().isZero()) {
    os << ", attrs=";
    P(p.get_attrs(), printUnsigned);
  }
#undef P
  return os << ')';
}


static vector<expr> extract_possible_local_bids(Memory &m, const Byte &b) {
  vector<expr> ret;
  expr zero = expr::mkUInt(0, bits_for_offset);
  for (auto ptr_val : expr::allLeafs(b.ptr_value())) {
    for (auto bid : expr::allLeafs(Pointer(m, move(ptr_val)).get_bid())) {
      Pointer ptr(m, bid, zero);
      if (!ptr.is_local().isFalse())
        ret.emplace_back(ptr.get_short_bid());
    }
  }
  return ret;
}

unsigned Memory::num_nonlocals() const {
  return state->isSource() ? num_nonlocals_src : IR::num_nonlocals;
}

void Memory::store(const Pointer &p, const expr &val, expr &local,
                   expr &non_local, bool index_bid) {
  if (!index_bid && num_locals > 0) {
    Byte byte(*this, expr(val));
    if (byte.is_ptr().isTrue()) {
      uint64_t bid;
      for (const auto &bid_expr : extract_possible_local_bids(*this, byte)) {
        if (bid_expr.isUInt(bid)) {
          if (bid < num_locals)
            escaped_local_blks[bid] = true;
        } else {
          // may escape a local ptr, but we don't know which one
          escaped_local_blks.clear();
          escaped_local_blks.resize(num_locals, true);
        }
      }
    }
  }
  auto is_local = p.is_local();
  auto idx = index_bid ? p.get_short_bid() : p.short_ptr();
  local = expr::mkIf(is_local, local.store(idx, val), local);
  non_local = expr::mkIf(!is_local, non_local.store(idx, val), non_local);
}

static void store_lambda(const Pointer &p, const expr &cond, const expr &val,
                         expr &local, expr &non_local) {
  auto is_local = p.is_local();
  auto idx = p.short_ptr();

  if (!is_local.isFalse())
    local = expr::mkLambda({ idx },
              expr::mkIf(is_local && cond, val, local.load(idx)));

  if (!is_local.isTrue())
    non_local = expr::mkLambda({ idx },
                  expr::mkIf(!is_local && cond, val, non_local.load(idx)));
}

static expr load(const Pointer &p, expr &local, expr &non_local) {
  auto idx = p.short_ptr();
  return expr::mkIf(p.is_local(), local.load(idx), non_local.load(idx));
}

// Global block id 0 is reserved for a null block.
static unsigned last_local_bid = 0;
static unsigned last_nonlocal_bid = 1;

static bool memory_unused() {
  return num_locals == 0 && num_nonlocals == 0;
}

static expr mk_block_val_array() {
  return expr::mkArray("blk_val",
                       expr::mkUInt(0, Pointer::total_bits_short()),
                       expr::mkUInt(0, Byte::bitsByte()));
}

static expr mk_liveness_array() {
  return expr::mkArray("blk_liveness", expr::mkUInt(0, bits_shortbid()), true);
}

static void mk_nonlocal_val_axioms(State &s, Memory &m, expr &val) {
  if (!does_ptr_mem_access)
    return;

  auto idx = Pointer(m, "#idx", false, false, expr()).short_ptr();
#if 0
  if (m.num_nonlocals() > 0) {
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
    assert(ptr_val.ptr().is_local().isFalse());

    val = expr::mkLambda({ idx }, expr::mkIf(is_ptr, ptr_val(), int_val));

    s.addAxiom(expr::mkForAll({ idx },
                    ptr_val.ptr().get_short_bid().ule(m.num_nonlocals() - 1)));
  } else {
    Byte byte(m, val.load(idx));
    s.addAxiom(expr::mkForAll({ idx }, !byte.is_ptr()));
  }
#else
  Byte byte(m, val.load(idx));
  Pointer loadedptr = byte.ptr();
  expr bid = loadedptr.get_short_bid();
  s.addAxiom(
    expr::mkForAll({ idx },
      byte.is_ptr().implies(!loadedptr.is_local() &&
                            !loadedptr.is_nocapture() &&
                            bid.ule(m.num_nonlocals() - 1))));
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
    = expr::mkConstArray(expr::mkUInt(0, Pointer::total_bits_short()),
                         Byte::mkPoisonByte(*this)());

  // all local blocks are dead in the beginning
  local_block_liveness
    = expr::mkConstArray(expr::mkUInt(0, bits_shortbid()), false);

  // A memory space is separated into non-local area / local area.
  // Non-local area is the lower half of memory (to include null pointer),
  // local area is the upper half.
  // This helps efficient encoding of disjointness between local and nonlocal
  // blocks.
  // The last byte of the memory cannot be allocated because ptr + size
  // (where size is the size of the block and ptr is the beginning address of
  // the block) should not overflow.

  // Initialize a memory block for null pointer.
  if (IR::num_nonlocals > 0)
    alloc(expr::mkUInt(0, bits_size_t), bits_program_pointer, GLOBAL, false,
          false, 0);

  escaped_local_blks.resize(num_locals, false);

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
  for (unsigned bid = 1; bid < IR::num_nonlocals; ++bid) {
    state->addAxiom(Pointer(*this, bid, false).block_alignment().ule(
                      Pointer(other, bid, false).block_alignment()));
  }

  if (!observes_addresses())
    return;

  if (IR::num_nonlocals > 0)
    state->addAxiom(Pointer::mkNullPointer(*this).get_address(false) == 0);

  // Non-local blocks are disjoint.
  // Ignore null pointer block
  for (unsigned bid = 1; bid < IR::num_nonlocals; ++bid) {
    Pointer p1(*this, bid, false);
    expr disj = p1.get_address() != 0;

    // Ensure block doesn't spill to local memory
    auto bit = bits_size_t - 1;
    disj &= (p1.get_address() + p1.block_size()).extract(bit, bit) == 0;

    // disjointness constraint
    for (unsigned bid2 = bid + 1; bid2 < IR::num_nonlocals; ++bid2) {
      Pointer p2(*this, bid2, false);
      disj &= p2.is_block_alive()
                .implies(disjoint(p1.get_address(), p1.block_size(),
                                  p2.get_address(), p2.block_size()));
    }
    state->addAxiom(p1.is_block_alive().implies(disj));
  }

  // ensure locals fit in their reserved space
  auto locals_fit = [](const Memory &m) {
    auto sum = expr::mkUInt(0, bits_size_t - 1);
    for (unsigned bid = 0; bid < num_locals; ++bid) {
      Pointer p(m, bid, true);
      if (auto sz = m.local_blk_size.lookup(p.get_short_bid())) {
        auto size = sz->extract(bits_size_t - 2, 0);
        auto align = expr::mkUInt(1, bits_size_t - 1) << p.block_alignment();
        align = align - expr::mkUInt(1, align.bits());
        m.state->addOOM(size.add_no_uoverflow(align));
        m.state->addOOM(sum.add_no_uoverflow(size + align));
        sum = sum + size + align;
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

expr Memory::mkInput(const char *name, unsigned attributes) const {
  Pointer p(*this, name, false, false, attr_to_bitvec(attributes));
  if (attributes & Input::NonNull)
    state->addAxiom(p.isNonZero());
  state->addAxiom(p.get_short_bid().ule(num_nonlocals() - 1));

  return p.release();
}

pair<expr, expr> Memory::mkUndefInput(unsigned attributes) const {
  bool nonnull = attributes & Input::NonNull;
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
            attr_to_bitvec(attributes));
  return { p.release(), move(undef) };
}

pair<expr,expr>
Memory::mkFnRet(const char *name, const vector<StateValue> &ptr_inputs) const {
  expr var
    = expr::mkFreshVar(name, expr::mkUInt(0, bits_for_bid + bits_for_offset));
  Pointer p(*this, var.concat_zeros(bits_for_ptrattrs));

  set<expr> local;
  for (auto &in : ptr_inputs) {
    Pointer inp(*this, in.value);
    if (!inp.is_local().isFalse())
      local.emplace(in.non_poison && p.get_bid() == inp.get_bid());
  }
  for (unsigned i = 0; i < num_locals; ++i) {
    if (escaped_local_blks[i])
      local.emplace(p.get_short_bid() == i);
  }

  state->addAxiom(expr::mkIf(p.is_local(),
                             expr::mk_or(local),
                             p.get_short_bid().ule(num_nonlocals() - 1)));
  return { p.release(), move(var) };
}

expr Memory::CallState::implies(const CallState &st) const {
  if (empty || st.empty)
    return true;
  // NOTE: using equality here is an approximation.
  // TODO: benchmark using quantifiers to state implication
  expr ret(true);
  if (block_val_var.isValid() && st.block_val_var.isValid())
    ret &= block_val_var == st.block_val_var;
  if (liveness_var.isValid() && st.liveness_var.isValid())
    ret &= liveness_var == st.liveness_var;
  return ret;
}

Memory::CallState
Memory::mkCallState(const vector<StateValue> *ptr_inputs) const {
  Memory::CallState st;
  st.empty = false;

  auto blk_val = mk_block_val_array();
  st.block_val_var = expr::mkFreshVar("blk_val", blk_val);
  st.liveness_var = expr::mkFreshVar("blk_liveness", mk_liveness_array());

  {
    Pointer p(*this, "#idx", false);
    expr modifies(true);

    if (ptr_inputs) {
      modifies = false;
      for (auto &arg : *ptr_inputs) {
        Pointer argp(*this, arg.value);
        modifies |= arg.non_poison && argp.get_bid() == p.get_bid();
      }
    }

    st.non_local_block_val
      = initial_non_local_block_val.subst(blk_val, st.block_val_var);

    if (!modifies.isTrue()) {
      auto idx = p.short_ptr();
      st.non_local_block_val
        = expr::mkLambda({ idx },
                         expr::mkIf(modifies, st.non_local_block_val.load(idx),
                                    non_local_block_val.load(idx)));
    }
  }
  {
    auto bid = expr::mkFreshVar("#idx", expr::mkUInt(0, bits_shortbid()));
    Pointer p(*this,
              prepend_if(expr::mkUInt(0, 1), expr(bid), ptr_has_local_bit()),
              expr::mkUInt(0, bits_for_offset));

    expr modifies = p.is_heap_allocated();
    if (ptr_inputs) {
      expr c(false);
      for (auto &arg : *ptr_inputs) {
        c |= arg.non_poison &&
             Pointer(*this, arg.value).get_bid() == p.get_bid();
      }
      modifies &= c;
    }

    if (modifies.isFalse()) {
      st.non_local_block_liveness = non_local_block_liveness;
      st.liveness_var = expr();
    } else {
      // functions can free an object, but cannot bring a dead one back to live
      st.non_local_block_liveness
        = expr::mkLambda({ bid }, non_local_block_liveness.load(bid) &&
                                  modifies.implies(st.liveness_var.load(bid)));
    }
  }
  return st;
}

void Memory::setState(const Memory::CallState &st) {
  non_local_block_val = st.non_local_block_val;
  non_local_block_liveness = st.non_local_block_liveness;
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
    disj &= p2.is_block_alive()
              .implies(disjoint(addr, sz, p2.get_address(), p2.block_size()));
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
  assert((is_local && bid < num_locals) ||
         (!is_local && bid < num_nonlocals()));
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
  auto short_bid = p.get_short_bid();
  // TODO: If address space is not 0, the address can be 0.
  // TODO: add support for C++ allocs
  unsigned alloc_ty = blockKind == HEAP ? Pointer::MALLOC : Pointer::NON_HEAP;

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
        state->addPre(full_addr.add_no_uoverflow(size_zext));

      // Disjointness of block's address range with other local blocks
      state->addPre(
        allocated.implies(disjoint_local_blocks(*this, full_addr, size_zext,
                                                local_blk_addr)));

      local_blk_addr.add(short_bid, move(blk_addr));
    }
  } else {
    state->addAxiom(p.block_size() == size_zext);
    state->addAxiom(p.is_block_aligned(align, true));
    state->addAxiom(p.get_alloc_type() == alloc_ty);

    if (align_bits && observes_addresses())
      state->addAxiom(p.get_address().extract(align_bits - 1, 0) == 0);

    if (blockKind == CONSTGLOBAL)
      non_local_blk_nonwritable.emplace_back(bid);
  }

  store(p, allocated, local_block_liveness, non_local_block_liveness, true);
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

void Memory::start_lifetime(const expr &ptr_local) {
  assert(!memory_unused());
  Pointer p(*this, ptr_local);
  state->addUB(p.is_local());

  if (observes_addresses())
    state->addPre(disjoint_local_blocks(*this, p.get_address(), p.block_size(),
                  local_blk_addr));

  local_block_liveness = local_block_liveness.store(p.get_short_bid(), true);
}

void Memory::free(const expr &ptr, bool unconstrained) {
  assert(!memory_unused() && (has_free || has_dead_allocas));
  Pointer p(*this, ptr);
  if (!unconstrained)
    state->addUB(p.isNull() || (p.get_offset() == 0 &&
                                p.is_block_alive() &&
                                p.get_alloc_type() == Pointer::MALLOC));
  store(p, false, local_block_liveness, non_local_block_liveness, true);
}

unsigned Memory::getStoreByteSize(const Type &ty) {
  if (ty.isPtrType())
    return divide_up(bits_program_pointer, 8);

  if (auto aty = ty.getAsAggregateType()) {
    unsigned sz = 0;
    for (unsigned i = 0; i < aty->numElementsConst(); ++i)
      sz += getStoreByteSize(aty->getChild(i));
    return sz;
  }
  return divide_up(ty.bits(), 8);
}

void Memory::store(const expr &p, const StateValue &v, const Type &type,
                   unsigned align, bool deref_check) {
  assert(!memory_unused());
  Pointer ptr(*this, p);
  unsigned bytesz = bits_byte / 8;

  if (deref_check)
    ptr.is_dereferenceable(getStoreByteSize(type), align,
                           !state->isInitializationPhase());

  if (auto aty = type.getAsAggregateType()) {
    unsigned byteofs = 0;
    for (unsigned i = 0, e = aty->numElementsConst(); i < e; ++i) {
      auto &child = aty->getChild(i);
      if (child.bits() == 0)
        continue;
      auto ptr_i = ptr + byteofs;
      auto align_i = gcd(align, byteofs % align);
      store(ptr_i(), aty->extract(v, i), child, align_i, false);
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

StateValue Memory::load(const expr &p, const Type &type, unsigned align,
                        bool deref_check) {
  assert(!memory_unused());
  unsigned bytesz = bits_byte / 8;
  unsigned bytecount = getStoreByteSize(type);

  Pointer ptr(*this, p);
  if (deref_check)
    ptr.is_dereferenceable(bytecount, align, false);

  StateValue ret;
  if (auto aty = type.getAsAggregateType()) {
    vector<StateValue> member_vals;
    unsigned byteofs = 0;
    for (unsigned i = 0, e = aty->numElementsConst(); i < e; ++i) {
      auto ptr_i = ptr + byteofs;
      auto align_i = gcd(align, byteofs % align);
      member_vals.push_back(load(ptr_i(), aty->getChild(i), align_i, false));
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
  return state->rewriteUndef(move(ret));
}

Byte Memory::load(const Pointer &p) {
  return { *this, ::load(p, local_block_val, non_local_block_val) };
}

// idx in [ptr, ptr+sz)
static expr ptr_deref_within(const Pointer &idx, const Pointer &ptr,
                             const expr &size) {
  expr ret = idx.get_short_bid() == ptr.get_short_bid();
  ret &= idx.uge(ptr).value;

  if (!size.zextOrTrunc(bits_size_t).uge(ptr.block_size()).isTrue())
    ret &= idx.ult(ptr + size).value;

  return ret;
}

void Memory::memset(const expr &p, const StateValue &val, const expr &bytesize,
                    unsigned align) {
  assert(!memory_unused());
  assert(!val.isValid() || val.bits() == 8);
  unsigned bytesz = bits_byte / 8;
  Pointer ptr(*this, p);
  ptr.is_dereferenceable(bytesize, align, true);

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
    Pointer idx(*this, "#idx", ptr.is_local());
    store_lambda(idx, ptr_deref_within(idx, ptr, bytesize), bytes[0](),
                 local_block_val, non_local_block_val);
  }
}

void Memory::memcpy(const expr &d, const expr &s, const expr &bytesize,
                    unsigned align_dst, unsigned align_src, bool is_move) {
  assert(!memory_unused());
  assert(bits_byte == 8);
  Pointer dst(*this, d), src(*this, s);
  dst.is_dereferenceable(bytesize, align_dst, true);
  src.is_dereferenceable(bytesize, align_src, false);
  if (!is_move)
    src.is_disjoint(bytesize, dst, bytesize);

  // copy to itself
  if ((src == dst).isTrue())
    return;

  uint64_t n;
  if (bytesize.isUInt(n) && n <= 4) {
    auto old_local = local_block_val, old_nonlocal = non_local_block_val;
    for (unsigned i = 0; i < n; ++i) {
      store(dst + i, ::load(src + i, old_local, old_nonlocal),
              local_block_val, non_local_block_val);
    }
  } else {
    Pointer dst_idx(*this, "#idx", dst.is_local());
    Pointer src_idx = src + (dst_idx.get_offset() - dst.get_offset());
    store_lambda(dst_idx, ptr_deref_within(dst_idx, dst, bytesize),
                 ::load(src_idx, local_block_val, non_local_block_val),
                 local_block_val, non_local_block_val);
  }
}

expr Memory::ptr2int(const expr &ptr) {
  assert(!memory_unused());
  return Pointer(*this, ptr).get_address();
}

expr Memory::int2ptr(const expr &val) {
  assert(!memory_unused());
  // TODO
  return {};
}

pair<expr,Pointer> Memory::refined(const Memory &other,
                                   const vector<StateValue> *set_ptrs) const {
  if (IR::num_nonlocals <= 1)
    return { true, Pointer(*this, expr()) };

  assert(!memory_unused());
  Pointer ptr(*this, "#idx_refinement", false);
  expr ptr_bid = ptr.get_bid();
  expr offset = ptr.get_offset();
  expr ret(true);

  for (unsigned bid = 1; bid < IR::num_nonlocals_src; ++bid) {
    expr bid_expr = expr::mkUInt(bid, bits_for_bid);
    Pointer p(*this, bid_expr, offset);
    Pointer q(other, p());
    if ((p.is_byval() && q.is_byval()).isTrue())
      continue;
    ret &= (ptr_bid == bid_expr).implies(p.block_refined(q));
  }

#ifndef NDEBUG
  auto is_constglb = [](const Memory &m, unsigned bid) {
    return
      find(m.non_local_blk_nonwritable.begin(),
           m.non_local_blk_nonwritable.end(), bid) !=
        m.non_local_blk_nonwritable.end();
  };

  for (unsigned bid = IR::num_nonlocals_src; bid < IR::num_nonlocals; ++bid)
    assert(!is_constglb(*this, bid) && is_constglb(other, bid));
#endif

  // restrict refinement check to set of request blocks
  if (set_ptrs) {
    expr c(false);
    for (auto &ptr : *set_ptrs) {
      c |= ptr.non_poison && Pointer(*this, ptr.value).get_bid() == ptr_bid;
    }
    ret = c.implies(ret);
  }

  return { move(ret), move(ptr) };
}

expr Memory::check_nocapture() const {
  if (!does_ptr_store)
    return true;

  auto name = local_name(state, "#offset_nocapture");
  auto ofs = expr::mkVar(name.c_str(), bits_for_offset);
  expr res(true);

  for (unsigned bid = 1; bid < num_nonlocals(); ++bid) {
    Pointer p(*this, expr::mkUInt(bid, bits_for_bid), ofs);
    Byte b(*this, non_local_block_val.load(p.short_ptr()));
    Pointer loadp(*this, b.ptr_value());
    res &= p.is_block_alive().implies(
             (b.is_ptr() && b.ptr_nonpoison()).implies(!loadp.is_nocapture()));
  }
  if (!res.isTrue())
    state->addQuantVar(ofs);
  return res;
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
  ret.non_local_blk_nonwritable.insert(ret.non_local_blk_nonwritable.end(),
                                       els.non_local_blk_nonwritable.begin(),
                                       els.non_local_blk_nonwritable.end());
  ret.non_local_blk_size.add(els.non_local_blk_size);
  ret.non_local_blk_align.add(els.non_local_blk_align);
  ret.non_local_blk_kind.add(els.non_local_blk_kind);
  ret.byval_blks.insert(ret.byval_blks.end(), els.byval_blks.begin(),
                        els.byval_blks.end());
  for (unsigned i = 0; i < num_locals; ++i) {
    if (els.escaped_local_blks[i])
      ret.escaped_local_blks[i] = true;
  }
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
        non_local_blk_kind, byval_blks, escaped_local_blks) <
    tie(rhs.non_local_block_val, rhs.local_block_val,
        rhs.initial_non_local_block_val,
        rhs.non_local_block_liveness, rhs.local_block_liveness,
        rhs.local_blk_addr, rhs.local_blk_size, rhs.local_blk_align,
        rhs.local_blk_kind, rhs.non_local_blk_nonwritable,
        rhs.non_local_blk_size, rhs.non_local_blk_align,
        rhs.non_local_blk_kind, rhs.byval_blks, rhs.escaped_local_blks);
}

#define P(msg, local, nonlocal)                                                \
  os << msg "\n";                                                              \
  if (num_locals > 0) os << "Local: " << m.local.simplify() << '\n';           \
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
  if (num_locals > 0) {
    os << "ESCAPED LOCAL BLOCKS: ";
    for (unsigned i = 0; i < num_locals; ++i) {
      os << m.escaped_local_blks[i];
    }
    os << "\nLOCAL BLOCK ADDR: " << m.local_blk_addr << '\n';
  }
  return os;
}

}
