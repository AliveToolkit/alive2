// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/memory.h"
#include "ir/state.h"
#include "util/compiler.h"

using namespace smt;
using namespace std;
using namespace util;

namespace IR {

Pointer::Pointer(Memory &m, unsigned bid, bool local) : m(m) {
  expr bid_expr;
  if (local)
    bid_expr = expr::mkUInt((uint64_t)bid << m.bits_for_nonlocal_bid,
                            bits_for_bids());
  else
    bid_expr = expr::mkUInt(bid, bits_for_bids());
  p = expr::mkUInt(0, m.bits_for_offset).concat(bid_expr);
}

unsigned Pointer::bits_for_bids() const {
  return m.bits_for_local_bid + m.bits_for_nonlocal_bid;
}

expr Pointer::get_bid() const {
  return p.extract(bits_for_bids() - 1, 0);
}

expr Pointer::get_offset() const {
  return p.extract(bits_for_bids() + m.bits_for_offset - 1, bits_for_bids());
}

void Pointer::operator++(void) {
  p = (get_offset() + expr::mkUInt(1, m.bits_for_offset)).concat(get_bid());
}

Pointer Pointer::operator+(const expr &bytes) const {
  return { m, (get_offset() + bytes).concat(get_bid()) };
}

expr Pointer::ult(const Pointer &rhs) const {
  return get_bid() == rhs.get_bid() && get_offset().ult(rhs.get_offset());
}

expr Pointer::uge(const Pointer &rhs) const {
  return get_bid() == rhs.get_bid() && get_offset().uge(rhs.get_offset());
}

void Pointer::is_dereferenceable(const expr &bytes) {
  expr block_sz = m.blocks_size.load(get_bid());
  expr offset = get_offset();

  // 1) check that offset is within bounds and that arith doesn't overflow
  m.state->addUB((offset + bytes).zextOrTrunc(m.bits_size_t).ult(block_sz));
  m.state->addUB(offset.add_no_uoverflow(bytes));

  // 2) check block is alive
  // TODO
}

void Pointer::is_dereferenceable(unsigned bytes) {
  is_dereferenceable(expr::mkUInt(bytes, m.bits_for_offset));
}


Memory::Memory(State &state) : state(&state) {
  unsigned bits_bids = bits_for_local_bid + bits_for_nonlocal_bid;
  blocks_size = expr::mkArray("blks_size",
                              expr::mkUInt(0, bits_bids),
                              expr::mkUInt(0, bits_size_t));

  blocks_val = expr::mkArray("blks_val",
                             expr::mkUInt(0, bits_bids + bits_for_offset),
                             expr::mkUInt(0, 8 + 1)); // val+poison bit

  assert(bits_for_offset <= bits_size_t);
}

expr Memory::alloc(const expr &bytes, unsigned align, bool local) {
  Pointer p(*this, last_bid++, local);
  // TODO: handle alignment
  blocks_size = blocks_size.store(p(), bytes);
  memset(p(), { expr::mkUInt(0, 8), false }, bytes, align);
  return p();
}

void Memory::free(const expr &ptr) {
  // TODO
}

void Memory::store(const expr &p, const StateValue &v, unsigned align) {
  unsigned bits = v.value.bits();
  unsigned bytes = divide_up(bits, 8);

  Pointer ptr(*this, p);
  // TODO: handle alignment
  ptr.is_dereferenceable(bytes);

  expr poison = v.non_poison.toBVBool();
  expr val = v.value.zext(bytes * 8 - bits);

  for (unsigned i = 0; i < bytes; ++i) {
    // FIXME: right now we store in little-endian; consider others?
    expr data = val.extract((i + 1) * 8 - 1, i * 8);
    blocks_val = blocks_val.store(ptr(), poison.concat(data));
    ++ptr;
  }
}

StateValue Memory::load(const expr &p, unsigned bits, unsigned align) {
  unsigned bytes = divide_up(bits, 8);
  Pointer ptr(*this, p);
  // TODO: handle alignment
  ptr.is_dereferenceable(bytes);

  expr val, non_poison;
  bool first = true;

  for (unsigned i = 0; i < bytes; ++i) {
    expr pair = blocks_val.load(ptr());
    expr v = pair.extract(8-1, 0);
    expr p = pair.extract(8, 8) == expr::mkUInt(1, 1);

    if (first) {
      val = move(v);
      non_poison = move(p);
    } else {
      val = v.concat(val);
      non_poison &= p;
    }
    first = false;
    ++ptr;
  }

  return { val.trunc(bits), move(non_poison) };
}

void Memory::memset(const expr &p, const StateValue &val, const expr &bytes,
                    unsigned align) {
  Pointer ptr(*this, p);
  // TODO: handle alignment
  ptr.is_dereferenceable(bytes);
  expr store_val = val.non_poison.toBVBool().concat(val.value);

  uint64_t n;
  if (bytes.isUInt(n) && n <= 4) {
    for (unsigned i = 0; i < n; ++i) {
      blocks_val = blocks_val.store(ptr(), store_val);
      ++ptr;
    }
  } else {
    string name = "#idx_" + to_string(last_idx_ptr++);
    Pointer idx(*this, expr::mkVar(name.c_str(), ptr.bits()));

    expr cond = idx.uge(ptr) && idx.ult(ptr + bytes);
    expr val = expr::mkIf(cond, blocks_val.store(idx(), store_val), blocks_val);
    blocks_val = expr::mkLambda({ idx() }, move(val));
  }
}

void Memory::memcpy(const expr &d, const expr &s, const expr &bytes,
                    unsigned align_dst, unsigned align_src) {
  Pointer dst(*this, d), src(*this, s);
  // TODO: handle alignment
  dst.is_dereferenceable(bytes);
  src.is_dereferenceable(bytes);
  // TODO
}

Memory Memory::mkIf(const expr &cond, const Memory &then, const Memory &els) {
  assert(then.state == els.state);
  Memory ret(then);
  ret.blocks_size  = expr::mkIf(cond, then.blocks_size, els.blocks_size);
  ret.blocks_val   = expr::mkIf(cond, then.blocks_val, els.blocks_val);
  ret.last_bid     = max(then.last_bid, els.last_bid);
  ret.last_idx_ptr = max(then.last_idx_ptr, els.last_idx_ptr);
  return ret;
}

}
