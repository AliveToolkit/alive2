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

void Pointer::is_dereferenceable(unsigned bytes) {
  expr block_sz = m.blocks_size.load(get_bid());
  expr offset = get_offset();
  expr length = expr::mkUInt(bytes, m.bits_for_offset);

  // 1) check that offset is within bounds and that arith doesn't overflow
  m.state.addUB((offset + length).ult(block_sz));
  m.state.addUB(offset.add_no_uoverflow(length));

  // 2) check block is alive
  // TODO
}


Memory::Memory(State &state) : state(state) {
  unsigned bits_bids = bits_for_local_bid + bits_for_nonlocal_bid;
  blocks_size = expr::mkArray("blks_size",
                              expr::mkUInt(0, bits_bids),
                              expr::mkUInt(0, bits_size_t));

  blocks_val = expr::mkArray("blks_val",
                             expr::mkUInt(0, bits_bids + bits_for_offset),
                             expr::mkUInt(0, byte_size + 1)); // val+poison bit
}

expr Memory::alloc(const expr &bytes, bool local) {
  Pointer p(*this, last_bid++, local);
  blocks_size = blocks_size.store(p(), bytes.zextOrTrunc(bits_size_t));
  memset(p(), { expr::mkUInt(0, byte_size), false }, bytes);
  return p();
}

void Memory::store(const expr &p, const StateValue &v) {
  unsigned bits = v.value.bits();
  unsigned bytes = divide_up(bits, byte_size);

  Pointer ptr(*this, p);
  ptr.is_dereferenceable(bytes);

  expr poison = v.non_poison.toBVBool();
  expr val = v.value.zext(bytes * byte_size - bits);

  for (unsigned i = 0; i < bytes; ++i) {
    // FIXME: right now we store in little-endian; consider others?
    expr data = val.extract((i + 1) * byte_size - 1, i * byte_size);
    blocks_val = blocks_val.store(ptr(), poison.concat(data));
    ++ptr;
  }
}

StateValue Memory::load(const expr &p, unsigned bits) {
  unsigned bytes = divide_up(bits, byte_size);
  Pointer ptr(*this, p);
  ptr.is_dereferenceable(bytes);

  expr val, non_poison;
  bool first = true;

  for (unsigned i = 0; i < bytes; ++i) {
    expr pair = blocks_val.load(ptr());
    expr v = pair.extract(byte_size-1, 0);
    expr p = pair.extract(byte_size, byte_size) == expr::mkUInt(1, 1);

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

void Memory::memset(const expr &ptr, const StateValue &val, const expr &bytes) {
  // TODO
}

void Memory::memcpy(const expr &dst, const expr &src, const expr &bytes) {
  // TODO
}

Memory Memory::ite(const expr &cond, const Memory &then, const Memory &els) {
  assert(&then.state == &els.state);
  Memory ret(then.state);
  ret.bits_for_offset       = then.bits_for_offset;
  ret.bits_for_local_bid    = then.bits_for_local_bid;
  ret.bits_for_nonlocal_bid = then.bits_for_nonlocal_bid;
  ret.bits_size_t           = then.bits_size_t;
  ret.byte_size             = then.byte_size;

  ret.blocks_size = expr::mkIf(cond, then.blocks_size, els.blocks_size);
  ret.blocks_val  = expr::mkIf(cond, then.blocks_val, els.blocks_val);
  ret.last_bid    = max(then.last_bid, els.last_bid);
  return ret;
}

}
