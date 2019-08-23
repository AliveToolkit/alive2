// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/memory.h"
#include "ir/state.h"
#include "util/compiler.h"

using namespace smt;
using namespace std;
using namespace util;

namespace IR {

Pointer::Pointer(Memory &m, const char *var_name)
  : m(m), p(expr::mkVar(var_name, total_bits())) {}

Pointer::Pointer(Memory &m, unsigned bid, bool local) : m(m) {
  expr bid_expr;
  if (local)
    bid_expr = expr::mkUInt((uint64_t)bid << m.bits_for_nonlocal_bid,
                            bits_for_bids());
  else
    bid_expr = expr::mkUInt(bid, bits_for_bids());
  p = expr::mkUInt(0, m.bits_for_offset).concat(bid_expr);
}

Pointer::Pointer(Memory &m, const expr &offset, const expr &local_bid,
                 const expr &nonlocal_bid)
  : m(m), p(offset.concat(local_bid).concat(nonlocal_bid)) {}

unsigned Pointer::total_bits() const {
  return bits_for_bids() + m.bits_for_offset;
}

unsigned Pointer::bits_for_bids() const {
  return m.bits_for_local_bid + m.bits_for_nonlocal_bid;
}

expr Pointer::is_local() const {
  // we need to check both because of undef pointers
  return get_local_bid() != 0 && get_nonlocal_bid() == 0;
}

expr Pointer::get_bid() const {
  return p.extract(bits_for_bids() - 1, 0);
}

expr Pointer::get_local_bid() const {
  return p.extract(bits_for_bids() - 1, m.bits_for_nonlocal_bid);
}

expr Pointer::get_nonlocal_bid() const {
  return p.extract(m.bits_for_nonlocal_bid - 1, 0);
}

expr Pointer::get_offset() const {
  return p.extract(bits_for_bids() + m.bits_for_offset - 1, bits_for_bids());
}

expr Pointer::get_address() const {
  expr offset = get_offset().sextOrTrunc(m.bits_size_t);
  auto local_name = m.mkName("blks_addr");
  return
    offset +
      expr::mkIf(is_local(),
                 expr::mkUF(local_name.c_str(), { get_local_bid() }, offset),
                 expr::mkUF("blks_addr", { get_nonlocal_bid() }, offset));
}

expr Pointer::block_size() const {
  // ASSUMPTION: programs can only allocate up to half of address space
  // so the first bit of size is always zero.
  // We need this assumption to support negative offsets.
  expr range = expr::mkUInt(0, m.bits_size_t - 1);
  auto local_name = m.mkName("blks_size");
  return
    expr::mkUInt(0, 1).concat(
      expr::mkIf(is_local(),
                 expr::mkUF(local_name.c_str(), { get_local_bid() }, range),
                 expr::mkUF("blks_size", { get_nonlocal_bid() }, range)));
}

Pointer Pointer::operator+(const expr &bytes) const {
  return { m, (get_offset() + bytes).concat(get_bid()) };
}

Pointer Pointer::operator+(unsigned bytes) const {
  return *this + expr::mkUInt(bytes, m.bits_for_offset);
}

void Pointer::operator+=(const expr &bytes) {
  p = (get_offset() + bytes).concat(get_bid());
}

expr Pointer::add_no_overflow(const expr &offset) const {
  return get_offset().add_no_soverflow(offset);
}

expr Pointer::operator==(const Pointer &rhs) const {
  return get_bid() == rhs.get_bid() && get_offset() == rhs.get_offset();
}

expr Pointer::operator!=(const Pointer &rhs) const {
  return !operator==(rhs);
}

#define DEFINE_CMP(op)                                                      \
StateValue Pointer::op(const Pointer &rhs) const {                          \
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
  return get_offset().sextOrTrunc(m.bits_size_t).ule(block_size());
}

expr Pointer::is_aligned(unsigned align) const {
  if (auto bits = ilog2(align))
    return get_address().extract(bits-1, 0) == 0;
  return true;
}

void Pointer::is_dereferenceable(const expr &bytes, unsigned align) {
  expr block_sz = block_size();
  expr offset = get_offset();

  // 1) check that offset is within bounds and that arith doesn't overflow
  m.state->addUB((offset + bytes).zextOrTrunc(m.bits_size_t).ule(block_sz));
  m.state->addUB(offset.add_no_uoverflow(bytes));

  // 2) check block's address is aligned
  m.state->addUB(is_aligned(align));

  // 3) check block is alive
  // TODO
}

void Pointer::is_dereferenceable(unsigned bytes, unsigned align) {
  is_dereferenceable(expr::mkUInt(bytes, m.bits_for_offset), align);
}

expr disjoint(expr offset1, const expr &len1, expr offset2, const expr &len2) {
  return ((offset1+len1).ule(offset2) || (offset2+len2).ule(offset1));
}

void Pointer::is_disjoint(const expr &len1, const Pointer &ptr2, const expr &len2) const {
  m.state->addUB(get_bid() != ptr2.get_bid() || disjoint(get_offset(), len1, ptr2.get_offset(), len2));
}

ostream& operator<<(ostream &os, const Pointer &p) {
  os << "pointer(" << (p.is_local().simplify().isTrue() ? "local" : "non-local")
     << ", block_id=";
  p.get_bid().simplify().printUnsigned(os);
  os << ", offset=";
  p.get_offset().simplify().printSigned(os);
  return os << ')';
}


string Memory::mkName(const char *str, bool src) const {
  return string(str) + (src ? "_src" : "_tgt");
}

string Memory::mkName(const char *str) const {
  return mkName(str, state->isSource());
}

expr Memory::mk_val_array(const char *name) const {
  unsigned bits_bids = bits_for_local_bid + bits_for_nonlocal_bid;
  return expr::mkArray(name, expr::mkUInt(0, bits_bids + bits_for_offset),
                             expr::mkUInt(0, 8 + 1)); // val+poison bit
}

Memory::Memory(State &state) : state(&state) {
  blocks_val = mk_val_array("blks_val");
  {
    // initialize all local blocks as poison
    Pointer idx(*this, "#idx0");
    expr poison = expr::mkUInt(0, 9);
    expr val = expr::mkIf(idx.is_local(), poison, blocks_val.load(idx()));
    blocks_val = expr::mkLambda({ idx() }, move(val));
  }

  assert(bits_for_offset <= bits_size_t);
}

pair<expr, vector<expr>> Memory::mkInput(const char *name) {
  unsigned bits = bits_for_nonlocal_bid + bits_for_offset;
  expr var = expr::mkVar(name, bits);
  expr offset = var.extract(bits - 1, bits_for_nonlocal_bid);
  expr bid = var.extract(bits_for_nonlocal_bid - 1, 0);
  expr local_bid = expr::mkUInt(0, bits_for_local_bid);
  return { Pointer(*this, offset, local_bid, bid).release(), { var } };
}

expr Memory::alloc(const expr &bytes, unsigned align, bool local) {
  Pointer p(*this, ++last_bid, local);
  state->addPre(p.is_aligned(align));

  expr size = bytes.zextOrTrunc(bits_size_t);
  state->addPre(p.block_size() == size);
  return p();
}

void Memory::free(const expr &ptr) {
  // TODO
}

void Memory::store(const expr &p, const StateValue &v, Type &type,
                   unsigned align) {
  expr val = v.value;
  expr poison = v.non_poison.toBVBool();

  if (type.isIntType()) {
    // do nothing
  } else if (type.isFloatType()) {
    val = val.float2BV();
  } else if (type.isPtrType()) {
    // TODO
    val = expr();
  } else {
    UNREACHABLE();
  }

  unsigned bits = val.bits();
  unsigned bytes = divide_up(bits, 8);

  val = val.zext(bytes * 8 - bits);

  Pointer ptr(*this, p);
  ptr.is_dereferenceable(bytes, align);

  for (unsigned i = 0; i < bytes; ++i) {
    // FIXME: right now we store in little-endian; consider others?
    expr data = val.extract((i + 1) * 8 - 1, i * 8);
    auto p = (ptr + i).release();
    blocks_val = blocks_val.store(p, poison.concat(data));
  }
}

StateValue Memory::load(const expr &p, Type &type, unsigned align) {
  auto bits = type.bits();
  unsigned bytes = divide_up(bits, 8);
  Pointer ptr(*this, p);
  ptr.is_dereferenceable(bytes, align);

  expr val, non_poison;
  bool first = true;

  for (unsigned i = 0; i < bytes; ++i) {
    auto ptr_i = (ptr + i).release();
    expr pair = blocks_val.load(ptr_i);
    expr v = pair.extract(8-1, 0);
    expr p = pair.extract(8, 8) == 1;

    if (first) {
      val = move(v);
      non_poison = move(p);
    } else {
      val = v.concat(val);
      non_poison &= p;
    }
    first = false;
  }

  val = val.trunc(bits);
  if (type.isIntType()) {
    // do nothing
  } else if (type.isFloatType()) {
    val = val.BV2float(type.getDummyValue());
  } else if (type.isPtrType()) {
    // TODO
    val = expr();
  } else {
    UNREACHABLE();
  }

  return { move(val), move(non_poison) };
}

void Memory::memset(const expr &p, const StateValue &val, const expr &bytes,
                    unsigned align) {
  Pointer ptr(*this, p);
  ptr.is_dereferenceable(bytes, align);
  expr store_val = val.non_poison.toBVBool().concat(val.value);

  uint64_t n;
  if (bytes.isUInt(n) && n <= 4) {
    for (unsigned i = 0; i < n; ++i) {
      auto p = (ptr + i).release();
      blocks_val = blocks_val.store(p, store_val);
    }
  } else {
    string name = "#idx_" + to_string(last_idx_ptr++);
    Pointer idx(*this, name.c_str());

    expr cond = idx.uge(ptr).both() && idx.ult(ptr + bytes).both();
    expr val = expr::mkIf(cond, store_val, blocks_val.load(idx()));
    blocks_val = expr::mkLambda({ idx() }, move(val));
  }
}

void Memory::memcpy(const expr &d, const expr &s, const expr &bytes,
                    unsigned align_dst, unsigned align_src) {
  // TODO: Add ub condition - bytes == 0
  Pointer dst(*this, d), src(*this, s);
  dst.is_dereferenceable(bytes, align_dst);
  src.is_dereferenceable(bytes, align_src);
  src.is_disjoint(bytes, dst, bytes);

  uint64_t n;
  if (bytes.isUInt(n) && n <= 4) {
    for (unsigned i = 0; i < n; ++i) {
      auto src_i = (src + i).release();
      auto dst_i = (dst + i).release();
      blocks_val = blocks_val.store(dst_i, blocks_val.load(src_i));
    }
  } else {
    string name = "#idx_" + to_string(last_idx_ptr++);
    Pointer idx(*this, expr::mkVar(name.c_str(), dst.bits()));

    expr cond = idx.uge(dst).both() && idx.ult(dst + bytes).both();
    expr val = expr::mkIf(cond, blocks_val.load((src + idx.get_offset()).release()), blocks_val.load(idx()));
    blocks_val = expr::mkLambda({ idx() }, move(val));
  }
}

expr Memory::ptr2int(const expr &ptr) {
  return Pointer(*this, ptr).get_address();
}

expr Memory::int2ptr(const expr &val) {
  // TODO
  return {};
}

Memory Memory::mkIf(const expr &cond, const Memory &then, const Memory &els) {
  assert(then.state == els.state);
  Memory ret(then);
  ret.blocks_val   = expr::mkIf(cond, then.blocks_val, els.blocks_val);
  // FIXME: this isn't correct; should be a per function counter
  ret.last_bid     = max(then.last_bid, els.last_bid);
  ret.last_idx_ptr = max(then.last_idx_ptr, els.last_idx_ptr);
  return ret;
}

}
