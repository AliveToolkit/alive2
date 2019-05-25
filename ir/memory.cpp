// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/memory.h"
#include "ir/state.h"
#include "util/compiler.h"

using namespace smt;
using namespace std;
using namespace util;

namespace {
struct Pointer {
  IR::Memory &m;
  // [offset, local_bid, nonlocal_bid]
  const expr &p;

  Pointer(IR::Memory *m, const expr &p) : m(*m), p(p) {}

  expr get_offset() const {
    // FIXME
    return p.extract(0, 0);
  }

  const expr& operator()() const {
    return p;
  }

  void operator++(void) {
    // TODO
  }

  void is_dereferenceable(unsigned bytes) {
    // TODO
  }
};
}

namespace IR {

Memory::Memory(State &state) : state(state) {
  unsigned bits_bids = bits_for_local_bid + bits_for_nonlocal_bid;
  blocks_size = expr::mkArray("blks_size",
                              expr::mkUInt(0, bits_bids),
                              expr::mkUInt(0, bits_size_t));

  blocks_val = expr::mkArray("blks_val",
                             expr::mkUInt(0, bits_bids + bits_for_offset),
                             expr::mkUInt(0, 8 + 1)); // 1 byte + 1 bit poison
}

void Memory::store(const expr &p, const StateValue &v) {
  unsigned bits = v.value.bits();
  unsigned bytes = divide_up(bits, 8);

  Pointer ptr(this, p);
  ptr.is_dereferenceable(bytes);

  expr poison = v.non_poison.toBVBool();
  expr val = v.value.zext(bytes * 8 - bits);

  for (unsigned i = 0; i < bytes; ++i) {
    // FIXME: right now we store in little-endian; consider others?
    blocks_val = blocks_val.store(ptr(),
                                  poison.concat(val.extract((i+1)*8 - 1, i*8)));
    ++ptr;
  }
}

StateValue Memory::load(const expr &p, unsigned bits) {
  unsigned bytes = divide_up(bits, 8);
  Pointer ptr(this, p);
  ptr.is_dereferenceable(bytes);

  expr val, non_poison;
  bool first = true;

  for (unsigned i = 0; i < bytes; ++i) {
    expr pair = blocks_val.load(ptr());
    expr v = pair.extract(7, 0);
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

Memory Memory::ite(const expr &cond, const Memory &then, const Memory &els) {
  // TODO
  return then;
}

}
