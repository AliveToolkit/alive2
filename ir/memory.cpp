// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/memory.h"
#include "ir/globals.h"
#include "ir/state.h"
#include "ir/value.h"
#include "util/compiler.h"

using namespace IR;
using namespace smt;
using namespace std;
using namespace util;

static bool did_pointer_store = false;
static unsigned ptr_next_idx = 0;

static expr concat_if(const expr &ifvalid, expr &&e) {
  return ifvalid.isValid() ? ifvalid.concat(e) : move(e);
}

static expr prepend_if(const expr &pre, expr &&e, bool prepend) {
  return prepend ? pre.concat(e) : move(e);
}

namespace IR {

Byte::Byte(const Memory &m, expr &&byterepr) : m(m), p(move(byterepr)) {
  assert(!p.isValid() || p.bits() == bitsByte());
}

Byte::Byte(const Pointer &ptr, unsigned i, const expr &non_poison)
  : m(ptr.getMemory()) {
  // TODO: support pointers larger than 64 bits.
  assert(bits_size_t <= 64 && bits_size_t % 8 == 0);

  if (!does_ptr_mem_access) {
    p = expr::mkUInt(0, bitsByte());
    return;
  }

  unsigned padding = bitsByte() - does_int_mem_access - 1
                                - Pointer::total_bits() - 3;
  if (does_int_mem_access) {
    p = expr::mkUInt(1, 1);
    if (padding)
      p = concat_if(p, expr::mkUInt(0, padding));
  }
  p = concat_if(p, non_poison.toBVBool())
        .concat(ptr()).concat(expr::mkUInt(i, 3));
  assert(!p.isValid() || p.bits() == bitsByte());
}

Byte::Byte(const Memory &m, const expr &data, const expr &non_poison) : m(m) {
  assert(!data.isValid() || data.bits() == bits_byte);
  assert(!non_poison.isValid() || non_poison.bits() == bits_byte);

  if (!does_int_mem_access) {
    p = expr::mkUInt(0, bitsByte());
    return;
  }

  unsigned padding = bitsByte() - 2 * bits_byte;
  p = non_poison.concat(data);
  if (padding)
    p = expr::mkUInt(0, padding).concat(p);
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
  auto bit = Pointer::total_bits() + 3;
  return p.extract(bit, bit) == 1;
}

Pointer Byte::ptr() const {
  return { m, ptr_value() };
}

expr Byte::ptr_value() const {
  if (!does_ptr_mem_access)
    return expr::mkUInt(0, Pointer::total_bits());
  return p.extract(Pointer::total_bits() + 3 - 1, 3);
}

expr Byte::ptr_byteoffset() const {
  if (!does_ptr_mem_access)
    return expr::mkUInt(0, 3);
  return p.extract(2, 0);
}

expr Byte::nonptr_nonpoison() const {
  if (!does_int_mem_access)
    return expr::mkUInt(0, bits_byte);
  return p.extract(bits_byte * 2 - 1, bits_byte);
}

expr Byte::nonptr_value() const {
  if (!does_int_mem_access)
    return expr::mkUInt(0, bits_byte);
  return p.extract(bits_byte - 1, 0);
}

expr Byte::is_poison(bool fullbit) const {
  expr np = nonptr_nonpoison();
  return expr::mkIf(is_ptr(), !ptr_nonpoison(),
                              fullbit ? np == expr::mkInt(-1, np)
                                      : np != 0);
}

expr Byte::is_zero() const {
  return expr::mkIf(is_ptr(),
                    ptr_nonpoison() && ptr().isNull(),
                    nonptr_nonpoison() == 0 && nonptr_value() == 0);
}

unsigned Byte::bitsByte() {
  bool is_ptr_bit = does_int_mem_access && does_ptr_mem_access;
  unsigned ptr_bits = does_ptr_mem_access * (1 + Pointer::total_bits() + 3);
  unsigned int_bits = does_int_mem_access * (2 * bits_byte);
  // allow at least 1 bit if there's no memory access
  return max(1u, is_ptr_bit + max(ptr_bits, int_bits));
}

Byte Byte::mkPtrByte(const Memory &m, const expr &val) {
  assert(does_ptr_mem_access);
  unsigned padding = bitsByte() - does_int_mem_access - 1
                                - Pointer::total_bits() - 3;
  expr byte;
  if (does_int_mem_access)
    byte = expr::mkUInt(1, 1);
  if (padding)
    byte = concat_if(byte, expr::mkUInt(0, padding));
  return { m, concat_if(byte, expr(val)) };
}

Byte Byte::mkNonPtrByte(const Memory &m, const expr &val) {
  if (!does_int_mem_access)
    return { m, expr::mkUInt(0, bitsByte()) };

  unsigned padding = bitsByte() - 2 * bits_byte;
  expr byte = padding ? expr::mkUInt(0, padding).concat(val) : val;
  return { m, move(byte) };
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
      for (unsigned i = 0; i < bits_byte; ++i) {
        unsigned idx = bits_byte - i - 1;
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
                                 const Memory &mem) {
  vector<Byte> bytes;
  if (fromType.isPtrType()) {
    Pointer p(mem, val.value);
    unsigned bytesize = bits_size_t / bits_byte;

    for (unsigned i = 0; i < bytesize; ++i)
      bytes.emplace_back(p, i, val.non_poison);
  } else {
    StateValue bvval = fromType.toBV(val);
    unsigned bitsize = bvval.bits();
    unsigned bytesize = divide_up(bitsize, bits_byte);

    bvval = bvval.zext(bytesize * bits_byte - bitsize);

    for (unsigned i = 0; i < bytesize; ++i) {
      expr data  = bvval.value.extract((i + 1) * bits_byte - 1, i * bits_byte);
      expr np    = bvval.non_poison.extract((i + 1) * bits_byte - 1,
                                            i * bits_byte);
      bytes.emplace_back(mem, data, np);
    }
  }
  return bytes;
}

static StateValue bytesToValue(const Memory &m, const vector<Byte> &bytes,
                               const Type &toType) {
  assert(!bytes.empty());

  if (toType.isPtrType()) {
    assert(bytes.size() == bits_size_t / bits_byte);
    auto nullp = Pointer::mkNullPointer(m);
    expr loaded_ptr;
    // The result is not poison if all of these hold:
    // (1) There's no poison byte, and they are all pointer bytes
    // (2) All of the bytes have the same information
    // (3) Byte offsets should be correct
    // A zero integer byte is considered as a null pointer byte with any byte
    // offset.
    expr non_poison = true;

    for (unsigned i = 0, e = bytes.size(); i < e; ++i) {
      auto &b = bytes[i];
      expr ptr_value = expr::mkIf(b.is_ptr(), b.ptr_value(), nullp());

      if (i == 0) {
        loaded_ptr = move(ptr_value);
      } else {
        non_poison &= ptr_value == loaded_ptr;
      }
      non_poison &= expr::mkIf(b.is_ptr(), b.ptr_byteoffset() == i,
                                           b.nonptr_value() == 0);
      non_poison &= !b.is_poison(false);
    }
    return { move(loaded_ptr), move(non_poison) };

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
    return toType.fromBV(val.trunc(bitsize));
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


namespace IR {

Pointer::Pointer(const Memory &m, const char *var_name, const expr &local,
                 bool unique_name, const expr &attr) : m(m) {
  string name = var_name;
  if (unique_name)
    name += '!' + to_string(ptr_next_idx++);
  unsigned bits = total_bits() - ptr_has_local_bit() - bits_for_ptrattrs;
  p = prepend_if(local.toBVBool(), expr::mkVar(name.c_str(), bits),
                 ptr_has_local_bit());
  if (bits_for_ptrattrs)
    p = p.concat(attr.isValid() ? attr : expr::mkUInt(0, bits_for_ptrattrs));
  assert(!p.isValid() || p.bits() == total_bits());
}

Pointer::Pointer(const Memory &m, expr repr) : m(m), p(move(repr)) {
  assert(!p.isValid() || p.bits() == total_bits());
}

Pointer::Pointer(const Memory &m, unsigned bid, bool local)
  : m(m), p(
    prepend_if(expr::mkUInt(local, 1),
               expr::mkUInt(bid, bits_shortbid())
                 .concat(expr::mkUInt(0, bits_for_offset + bits_for_ptrattrs)),
               ptr_has_local_bit())) {
  assert((local && bid < num_locals) || (!local && bid < num_nonlocals));
  assert(p.bits() == total_bits());
}

Pointer::Pointer(const Memory &m, const expr &bid, const expr &offset)
  : m(m), p(bid.concat(offset).concat_zeros(bits_for_ptrattrs)) {
  assert(!p.isValid() || p.bits() == total_bits());
}

Pointer::Pointer(const Memory &m, const expr &bid, const expr &offset,
                 const expr &attr) : m(m) {
  p = bid.concat(offset);
  if (bits_for_ptrattrs)
    p = p.concat(attr.isValid() ? attr : expr::mkUInt(0, bits_for_ptrattrs));
  assert(!p.isValid() || p.bits() == total_bits());
}

unsigned Pointer::total_bits() {
  return bits_for_ptrattrs + bits_for_bid + bits_for_offset;
}

expr Pointer::is_local() const {
  if (num_locals == 0)
    return false;
  if (num_nonlocals == 0)
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
  return p.extract(bits_for_offset + bits_for_ptrattrs - 1, bits_for_ptrattrs)
          .sextOrTrunc(bits_size_t);
}

expr Pointer::get_attrs() const {
  return p.extract(bits_for_ptrattrs - 1, 0);
}

expr Pointer::get_value(const char *name, const FunctionExpr &local_fn,
                        const FunctionExpr &nonlocal_fn,
                        const expr &ret_type, bool src_name) const {
  auto bid = get_short_bid();
  expr non_local;

  if (auto val = nonlocal_fn.lookup(bid))
    non_local = *val;
  else {
    string uf = name;
    if (src_name)
      uf += m.state->isSource() ? "_src" : "_tgt";
    non_local = expr::mkUF(uf.c_str(), { bid }, ret_type);
  }

  if (auto local = local_fn(bid))
    return expr::mkIf(is_local(), *local, non_local);
  return non_local;
}

expr Pointer::get_address(bool simplify) const {
  assert(observes_addresses());

  auto bid = get_short_bid();
  auto zero = expr::mkUInt(0, bits_for_offset - 1);
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

  return addr + get_offset();
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
  return p.extract(total_bits() - 1 - ptr_has_local_bit(), bits_for_ptrattrs);
}

Pointer Pointer::operator+(const expr &bytes) const {
  expr off = (get_offset() + bytes.zextOrTrunc(bits_size_t));
  if (bits_for_ptrattrs)
    return { m, get_bid(), off.trunc(bits_for_offset), get_attrs() };
  else
    return { m, get_bid(), off.trunc(bits_for_offset) };
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

expr Pointer::inbounds() const {
  // equivalent to offset >= 0 && offset <= block_size
  // because block_size u<= 0x7FFF..
  return get_offset().ule(block_size());
}

expr Pointer::block_alignment() const {
  return get_value("blk_align", m.local_blk_align, m.non_local_blk_align,
                   expr::mkUInt(0, 8), true);
}

expr Pointer::is_block_aligned(unsigned align, bool exact) const {
  assert(align != 0);
  if (!exact && align == 1)
    return true;

  auto bits = ilog2(align);
  expr blk_align = block_alignment();
  return exact ? blk_align == bits : blk_align.uge(bits);
}

expr Pointer::is_aligned(unsigned align) const {
  assert(align != 0);
  if (align == 1)
    return true;

  auto bits = ilog2(align);

  if (!observes_addresses())
    // This is stricter than checking get_address(), but as addresses are not
    // observed, program shouldn't be able to distinguish this from checking
    // get_address()
    return is_block_aligned(align, false) &&
           get_offset().extract(bits - 1, 0) == 0;

  return get_address().extract(bits - 1, 0) == 0;
}

static pair<expr, expr> is_dereferenceable(const Pointer &p, const expr &bytes,
                                           unsigned align, bool iswrite) {
  expr block_sz = p.block_size();
  expr offset = p.get_offset();

  // check that offset is within bounds and that arith doesn't overflow
  expr cond = (offset + bytes).ule(block_sz);
  cond &= offset.add_no_uoverflow(bytes);

  cond &= p.is_block_alive();

  if (iswrite)
    cond &= p.is_writable();

  // try some constant folding; these are implied by the conditions above
  if (bytes.ugt(block_sz).isTrue() ||
      offset.uge(block_sz).isTrue())
    cond = false;

  return { move(cond), p.is_aligned(align) };
}

// When bytes is 0, pointer is always derefenceable
void Pointer::is_dereferenceable(const expr &bytes0, unsigned align,
                                 bool iswrite) {
  expr bytes = bytes0.zextOrTrunc(bits_size_t);
  DisjointExpr<expr> UB(expr(false)), is_aligned(expr(false)), all_ptrs;

  for (auto &[ptr_expr, domain] : DisjointExpr<expr>(p, true)) {
    Pointer ptr(m, ptr_expr);
    auto [ub, aligned] = ::is_dereferenceable(ptr, bytes, align, iswrite);

    // record pointer if not definitely unfeasible
    if (!ub.isFalse() && !aligned.isFalse() && !ptr.block_size().isZero())
      all_ptrs.add(ptr_expr, domain);

    UB.add(move(ub), domain);
    is_aligned.add(move(aligned), domain);
  }

  m.state->addUB(bytes == 0 || *UB());

  // address must be always aligned regardless of access size
  m.state->addUB(*is_aligned());

  // trim set of valid ptrs
  if (auto ptrs = all_ptrs())
    p = *ptrs;
  else
    p = expr::mkUInt(0, total_bits());
}

void Pointer::is_dereferenceable(unsigned bytes, unsigned align, bool iswrite) {
  is_dereferenceable(expr::mkUInt(bytes, bits_for_offset), align, iswrite);
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
                  disjoint(get_offset(),
                           len1.zextOrTrunc(bits_size_t),
                           ptr2.get_offset(),
                           len2.zextOrTrunc(bits_size_t)));
}

expr Pointer::is_block_alive() const {
  // If programs have no free(), we assume all blocks are always live.
  // For non-local blocks, there's enough non-determinism through block size,
  // that can be 0 or non-0
  if (!has_free && !has_dead_allocas)
    return true;

  auto bid = get_short_bid();
  return expr::mkIf(is_local(), m.local_block_liveness.load(bid),
                    m.non_local_block_liveness.load(bid));
}

expr Pointer::get_alloc_type() const {
  // If programs have no malloc & free, we don't need to store this information
  // since it is only used to check if free/delete is ok and
  // for memory refinement of local malloc'ed blocks
  if (!has_malloc && !has_free)
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
  expr off = get_offset();
  expr size2 = other.block_size();
  expr off2 = other.get_offset();

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
  expr int_cnstr = (val2.nonptr_nonpoison() | np1) == np1 &&
                   (val.nonptr_value() | np1) == (val2.nonptr_value() | np1);

  // fast path: if we didn't do any ptr store, then all ptrs in memory were
  // already there and don't need checking
  expr is_ptr = val.is_ptr();
  expr is_ptr2 = val2.is_ptr();
  expr ptr_cnstr;
  if (!did_pointer_store || is_ptr.isFalse() || is_ptr2.isFalse()) {
    ptr_cnstr = val == val2;
  } else {
    ptr_cnstr = val2.ptr_nonpoison() && val.ptr().refined(val2.ptr());
  }
  return val.is_poison() ||
         expr::mkIf(is_ptr == is_ptr2,
                    expr::mkIf(is_ptr, ptr_cnstr, int_cnstr),
                    val.is_zero() && val2.is_zero());
}

expr Pointer::block_refined(const Pointer &other) const {
  expr blk_size = block_size();
  expr val_refines(true);
  uint64_t bytes;

  if (blk_size.isUInt(bytes) && bytes <= 8) {
    expr bid = get_bid();
    expr ptr_offset = get_offset();

    for (unsigned off = 0; off < bytes; ++off) {
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
         (is_block_alive() && get_offset().ult(blk_size)).implies(val_refines);
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
  if (bits_for_ptrattrs == 0)
    return false;
  return p.extract(0, 0) == 1;
}

Pointer Pointer::mkNullPointer(const Memory &m) {
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

  os << "pointer(" << (p.is_local().isTrue() ? "local" : "non-local")
     << ", block_id=";
  p.get_bid().printUnsigned(os);
  os << ", offset=";
  p.get_offset().printSigned(os);

  if (bits_for_ptrattrs) {
    os << ", attrs=";
    p.get_attrs().printUnsigned(os);
  }
  return os << ')';
}


static void store(const Pointer &p, const expr &val, expr &local,
                  expr &non_local, bool index_bid = false) {
  // check if we are potentially storing a pointer
  if (!index_bid)
    did_pointer_store |= !Byte(p.getMemory(), expr(val)).is_ptr().isFalse();

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
  // +1 for the null block
  return num_locals == 0 && num_nonlocals == 1 && !nullptr_is_used &&
         !observes_addresses();
}

Memory::Memory(State &state) : state(&state) {
  if (memory_unused())
    return;

  non_local_block_val
    = expr::mkArray("blk_val",
                    expr::mkUInt(0, bits_shortbid() + bits_for_offset),
                    expr::mkUInt(0, Byte::bitsByte()));

  // Non-local blocks cannot initially contain pointers to local blocks
  // and no-capture pointers.
  if (does_ptr_mem_access) {
    auto idx = Pointer(*this, "#idx", false, false, expr()).short_ptr();
#if 0
    if (num_nonlocals > 0) {
      expr is_ptr = expr::mkUF("blk_init_isptr", { idx }, true);
      expr int_val
        = Byte::mkNonPtrByte(*this, expr::mkUF("blk_init_nonptr", { idx },
                                             expr::mkUInt(0, bits_byte * 2)))();

      expr np = expr::mkUF("blk_init_ptr_np", { idx }, expr::mkUInt(0, 1));
      expr bid_off = expr::mkUF("blk_init_ptr_bid_off", { idx },
        expr::mkUInt(0, bits_shortbid() + bits_for_offset + 3));
      bid_off = prepend_if(expr::mkUInt(0, 1), move(bid_off),
                           ptr_has_local_bit());
      Byte ptr_val = Byte::mkPtrByte(*this, np.concat(bid_off));
      assert(ptr_val.ptr().is_local().isFalse());

      non_local_block_val =
        expr::mkLambda({ idx }, expr::mkIf(is_ptr, ptr_val(), int_val));

      if (state.isSource())
        state.addAxiom(expr::mkForAll({ idx },
                         ptr_val.ptr().get_short_bid().ule(num_nonlocals - 1)));
    } else {
      Byte byte(*this, non_local_block_val.load(idx));
      state.addAxiom(expr::mkForAll({ idx }, !byte.is_ptr()));
    }
#else
    if (state.isSource()) {
      Byte byte(*this, non_local_block_val.load(idx));
      Pointer loadedptr = byte.ptr();
      expr bid = loadedptr.get_short_bid();
      state.addAxiom(
        expr::mkForAll({ idx },
          byte.is_ptr().implies(!loadedptr.is_local() &&
                                !loadedptr.is_nocapture() &&
                                bid.ule(num_nonlocals - 1))));
    }
#endif
  }

  non_local_block_liveness
    = expr::mkArray("blk_liveness", expr::mkUInt(0, bits_shortbid()), true);

  // initialize all local blocks as non-pointer, poison value
  // This is okay because loading a pointer as non-pointer is also poison.
  local_block_val
    = expr::mkConstArray(expr::mkUInt(0, bits_shortbid() + bits_for_offset),
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
  if (num_nonlocals > 0)
    alloc(expr::mkUInt(0, bits_size_t), 1, GLOBAL, false, false, 0);

  assert(bits_for_offset <= bits_size_t);
}

void Memory::mkAxioms(const Memory &other) const {
  assert(state->isSource() && !other.state->isSource());
  if (memory_unused())
    return;

  // transformation can increase alignment
  for (unsigned bid = 1; bid < num_nonlocals; ++bid) {
    state->addAxiom(Pointer(*this, bid, false).block_alignment().ule(
                      Pointer(other, bid, false).block_alignment()));
  }

  if (observes_addresses() && num_nonlocals > 0) {
    state->addAxiom(Pointer::mkNullPointer(*this).get_address(false) == 0);

    // Non-local blocks are disjoint.
    // Ignore null pointer block
    for (unsigned bid = 1; bid < num_nonlocals; ++bid) {
      Pointer p1(*this, bid, false);
      expr disj = p1.get_address() != 0;

      // Ensure block doesn't spill to local memory
      auto bit = bits_size_t - 1;
      disj &= (p1.get_address() + p1.block_size()).extract(bit, bit) == 0;

      // disjointness constraint
      for (unsigned bid2 = bid + 1; bid2 < num_nonlocals; ++bid2) {
        Pointer p2(*this, bid2, false);
        disj &= p2.is_block_alive()
                  .implies(disjoint(p1.get_address(), p1.block_size(),
                                    p2.get_address(), p2.block_size()));
      }
      state->addAxiom(p1.is_block_alive().implies(disj));
    }
  }

  // ensure globals fit in their reserved space
  {
    auto sum_globals = expr::mkUInt(0, bits_size_t - 1);

    for (unsigned bid = 1; bid < num_nonlocals; ++bid) {
      Pointer p(*this, bid, false);
      auto sz = p.block_size().extract(bits_size_t - 2, 0);
      state->addAxiom(p.is_block_alive()
                       .implies(sum_globals.add_no_uoverflow(sz)));
      sum_globals = expr::mkIf(p.is_block_alive(), sum_globals + sz,
                               sum_globals);
    }
  }

  // ensure locals fit in their reserved space
  auto locals_fit = [](const Memory &m) {
    auto sum = expr::mkUInt(0, bits_size_t - 1);
    for (unsigned bid = 0; bid < num_locals; ++bid) {
      Pointer p(m, bid, true);
      if (auto sz = m.local_blk_size.lookup(p.get_short_bid())) {
        auto size = sz->extract(bits_size_t - 2, 0);
        m.state->addPre(sum.add_no_uoverflow(size));
        sum = sum + size;
      }
    }
  };
  locals_fit(*this);
  locals_fit(other);
}

void Memory::resetGlobalData() {
  resetLocalBids();
  last_nonlocal_bid = 1;
  did_pointer_store = false;
}

void Memory::resetLocalBids() {
  last_local_bid = 0;
  ptr_next_idx = 0;
}

void Memory::markByVal(unsigned bid) {
  byval_blks.emplace_back(bid);
}

expr Memory::mkInput(const char *name, unsigned attributes) const {
  bool is_nocapture = (attributes & Input::NoCapture) != 0;
  Pointer p(*this, name, false, false,
            bits_for_ptrattrs ? expr::mkUInt(is_nocapture, bits_for_ptrattrs) :
                                expr());
  if (attributes & Input::NonNull)
    state->addAxiom(p.isNonZero());
  state->addAxiom(p.get_short_bid().ule(num_nonlocals - 1));

  return p.release();
}

pair<expr, expr> Memory::mkUndefInput(unsigned attributes) const {
  bool nonnull = attributes & Input::NonNull;
  unsigned log_offset = ilog2_ceil(bits_for_offset);
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
  bool is_nocapture = (attributes & Input::NoCapture) != 0;
  Pointer p(*this, expr::mkUInt(0, bits_for_bid), offset,
            bits_for_ptrattrs ? expr::mkUInt(is_nocapture, bits_for_ptrattrs) :
                                expr());
  return { p.release(), move(undef) };
}

expr Memory::mkFnRet(const char *name) const {
  // TODO: can only alias with escaped local blocks!
  Pointer p(*this, expr::mkVar(name, Pointer::total_bits()));
  state->addAxiom(expr::mkIf(p.is_local(),
                             p.get_short_bid().ule(num_locals - 1),
                             p.get_short_bid().ule(num_nonlocals - 1)));
  return p.release();
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
  assert((is_local && bid < num_locals) || (!is_local && bid < num_nonlocals));
  if (!bidopt)
    ++last_bid;
  assert(bid < last_bid);

  if (bid_out)
    *bid_out = bid;

  expr size_zext = size.zextOrTrunc(bits_size_t);
  expr nooverflow = size_zext.extract(bits_size_t - 1, bits_size_t - 1) == 0;

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

      expr blk_addr = align_bits ? addr_var.concat(expr::mkUInt(0, align_bits))
                                 : addr_var;

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

  ::store(p, allocated, local_block_liveness, non_local_block_liveness, true);
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
  ::store(p, false, local_block_liveness, non_local_block_liveness, true);
}

static unsigned getStoreByteSize(const Type &ty) {
  if (ty.isPtrType())
    return divide_up(bits_size_t, 8);

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
  assert(align % bytesz == 0);

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
      store(ptr_i(), aty->extract(v, i), child, 1, false);
      byteofs += getStoreByteSize(child);
    }
    assert(byteofs == getStoreByteSize(type));

  } else {
    vector<Byte> bytes = valueToBytes(v, type, *this);
    assert(!v.isValid() || bytes.size() * bytesz == getStoreByteSize(type));

    for (unsigned i = 0, e = bytes.size(); i < e; ++i) {
      auto ptr_i = ptr + (little_endian ? i * bytesz :
                                          (e - i - 1) * bytesz);
      ::store(ptr_i, bytes[i](), local_block_val, non_local_block_val);
    }
  }
}

StateValue Memory::load(const expr &p, const Type &type, unsigned align,
                        bool deref_check) {
  assert(!memory_unused());
  unsigned bytesz = bits_byte / 8;
  assert(align % bytesz == 0);
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
      member_vals.push_back(load(ptr_i(), aty->getChild(i), 1, false));
      byteofs += getStoreByteSize(aty->getChild(i));
    }
    assert(byteofs == bytecount);
    ret = aty->aggregateVals(member_vals);

  } else {
    vector<Byte> loadedBytes;
    bytecount /= bytesz;
    for (unsigned i = 0; i < bytecount; ++i) {
      auto ptr_i = ptr + (little_endian ? i * bytesz :
                                          (bytecount - i - 1) * bytesz);
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
  assert(bits_byte == 8);
  assert(!val.isValid() || val.bits() == 8);
  Pointer ptr(*this, p);
  ptr.is_dereferenceable(bytesize, align, true);

  vector<Byte> bytes = valueToBytes(val, IntType("", bits_byte), *this);
  assert(bytes.size() == 1);

  uint64_t n;
  if (bytesize.isUInt(n) && n <= 4) {
    for (unsigned i = 0; i < n; ++i) {
      ::store(ptr + i, bytes[0](), local_block_val, non_local_block_val);
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
      ::store(dst + i, ::load(src + i, old_local, old_nonlocal),
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

pair<expr,Pointer> Memory::refined(const Memory &other) const {
  if (num_nonlocals <= 1)
    return { true, Pointer(*this, expr()) };

  assert(!memory_unused());
  Pointer ptr(*this, "#idx_refinement", false, false);
  expr ptr_bid = ptr.get_bid();
  expr offset = ptr.get_offset();
  expr ret(true);

  for (unsigned bid = 1; bid < num_nonlocals; ++bid) {
    expr bid_expr = expr::mkUInt(bid, bits_for_bid);
    Pointer p(*this, bid_expr, offset);
    Pointer q(other, p());
    if ((p.is_byval() && q.is_byval()).isTrue())
      continue;
    ret &= (ptr_bid == bid_expr).implies(p.block_refined(q));
  }
  return { move(ret), move(ptr) };
}

expr Memory::check_nocapture() const {
  auto ofs = expr::mkFreshVar("ofs", expr::mkUInt(0, bits_for_offset));
  expr res(true);

  for (unsigned bid = 1; bid < num_nonlocals; ++bid) {
    Pointer p(*this, expr::mkUInt(bid, bits_for_bid), ofs);
    Byte b(*this, non_local_block_val.load(p.short_ptr()));
    Pointer loadp(*this, b.ptr_value());
    res &= p.is_block_alive().implies(
        expr::mkForAll({ ofs },
            (b.is_ptr() && b.ptr_nonpoison()).implies(!loadp.is_nocapture())));
  }
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
  return ret;
}

bool Memory::operator<(const Memory &rhs) const {
  // FIXME: remove this once we move to C++20
  return
    tie(non_local_block_val, local_block_val,
        non_local_block_liveness, local_block_liveness, local_blk_addr,
        local_blk_size, local_blk_align, local_blk_kind,
        non_local_blk_nonwritable, non_local_blk_size, non_local_blk_align,
        non_local_blk_kind, byval_blks) <
    tie(rhs.non_local_block_val, rhs.local_block_val,
        rhs.non_local_block_liveness, rhs.local_block_liveness,
        rhs.local_blk_addr, rhs.local_blk_size, rhs.local_blk_align,
        rhs.local_blk_kind, rhs.non_local_blk_nonwritable,
        rhs.non_local_blk_size, rhs.non_local_blk_align,
        rhs.non_local_blk_kind, rhs.byval_blks);
}

#define P(msg, local, nonlocal)                            \
  os << msg "\nLocal: " << m.local.simplify()              \
     << "\nNon-local: " << m.nonlocal.simplify() << "\n\n"

ostream& operator<<(ostream &os, const Memory &m) {
  if (memory_unused())
    return os;
  os << "\n\nMEMORY\n======\n";
  P("BLOCK VALUE:", local_block_val, non_local_block_val);
  P("BLOCK LIVENESS:", local_block_liveness, non_local_block_liveness);
  P("BLOCK SIZE:", local_blk_size, non_local_blk_size);
  P("BLOCK ALIGN:", local_blk_align, non_local_blk_align);
  P("BLOCK KIND:", local_blk_kind, non_local_blk_kind);
  return os << "LOCAL BLOCK ADDR: " << m.local_blk_addr
            << "\nDid pointer store: " << did_pointer_store << '\n';
}

}
