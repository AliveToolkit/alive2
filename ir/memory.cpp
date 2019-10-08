// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/memory.h"
#include "ir/state.h"
#include "util/compiler.h"

using namespace IR;
using namespace smt;
using namespace std;
using namespace util;

namespace {

// A data structure that represents a byte.
// A byte is either a pointer byte or a non-pointer byte.
// Pointer byte's representation:
//   +-+------------------+-------------------+----------------+-------------+
//   |1| non-poison byte? | ptr offset        | block id       | byte offset |
//   | | (1 bit)          | (bits_for_offset) | (bits_for_bid) | (3 bits)    |
//   +-+------------------+-------------------+----------------+-------------+
// Non-pointer byte's representation:
//   +-+--------------------------+--------------------+---------------------+
//   |0| padding(000000000)       | non-poison bits?   | data                |
//   | |                          | (8 bits)           | (8 bits)            |
//   +-+--------------------------+--------------------+---------------------+
class Byte {
  const Memory &m;
  expr p;

public:
  // Creates a byte with its raw representation.
  Byte(const Memory &m, expr &&byterepr) : m(m), p(move(byterepr)) {
    assert(p.bits() == m.bitsByte());
  }

  // Creates a pointer byte that represents i'th byte of p.
  // non_poison should be an one-bit vector or boolean.
  Byte(const Pointer &ptr, unsigned i, const expr &non_poison)
    : m(ptr.getMemory()) {
    // TODO: support pointers larger than 64 bits.
    assert(m.bitsPtrSize() <= 64 && m.bitsPtrSize() % 8 == 0);

    expr byteofs = expr::mkUInt(i, 3);
    expr np = non_poison.toBVBool();
    p = expr::mkUInt(1, 1).concat(np).concat(ptr()).concat(byteofs);
    assert(p.bits() == m.bitsByte());
  }

  // Creates a non-pointer byte that has data and non_poison.
  // data and non_poison should have 8 bits.
  Byte(const Memory &m, const expr &data, const expr &non_poison) : m(m) {
    assert(m.bitsByte() > 1 + 8 + 8);
    assert(data.bits() == 8);
    assert(non_poison.bits() == 8);

    unsigned padding = m.bitsByte() - 1 - 8 - 8;
    p = expr::mkUInt(0, 1).concat(expr::mkUInt(0, padding)).concat(non_poison)
                          .concat(data);
  }

  expr is_ptr() const {
    auto bit = p.bits() - 1;
    return p.extract(bit, bit) == 1;
  }

  // Assuming that this is a pointer byte
  expr ptr_nonpoison() const {
    auto bit = p.bits() - 2;
    return p.extract(bit, bit) == 1;
  }

  expr ptr_value() const { return p.extract(p.bits() - 3, 3); }
  expr ptr_byteoffset() const { return p.extract(2, 0); }

  // Assuming that this is a non-pointer byte
  expr nonptr_nonpoison() const { return p.extract(15, 8); };
  expr nonptr_value() const { return p.extract(7, 0); }

  const expr& operator()() const { return p; }
};


vector<Byte> valueToBytes(const StateValue &val, const Type &fromType,
                          const Memory &mem) {
  vector<Byte> bytes;
  if (fromType.isPtrType()) {
    Pointer p(mem, val.value);
    unsigned bytesize = mem.bitsPtrSize() / 8;

    for (unsigned i = 0; i < bytesize; ++i)
      bytes.emplace_back(p, i, val.non_poison);
  } else {
    StateValue bvval = fromType.toBV(val);
    unsigned bitsize = bvval.bits();
    unsigned bytesize = divide_up(bitsize, 8);

    bvval = bvval.zext(bytesize * 8 - bitsize);

    for (unsigned i = 0; i < bytesize; ++i) {
      expr data  = bvval.value.extract((i + 1) * 8 - 1, i * 8);
      expr np    = bvval.non_poison.extract((i + 1) * 8 - 1, i * 8);
      bytes.emplace_back(mem, data, np);
    }
  }
  return bytes;
}

StateValue bytesToValue(const vector<Byte> &bytes, const Type &toType) {
  assert(!bytes.empty());

  if (toType.isPtrType()) {
    bool first = true;

    expr loaded_ptr;
    // The result is not poison if all of these hold:
    // (1) There's no poison byte, and they are all pointer bytes
    // (2) All of the bytes have the same information
    // (3) Byte offsets should be correct
    expr non_poison = true;

    for (unsigned i = 0, e = bytes.size(); i < e; ++i) {
      auto &b = bytes[i];
      expr ptr_value = b.ptr_value();

      if (first) {
        loaded_ptr = move(ptr_value);
      } else {
        non_poison &= ptr_value == loaded_ptr;
      }
      non_poison &= b.is_ptr();
      non_poison &= b.ptr_nonpoison();
      non_poison &= b.ptr_byteoffset() == i;
      first = false;
    }
    return { move(loaded_ptr), move(non_poison) };

  } else {
    auto bitsize = toType.bits();
    assert(divide_up(bitsize, 8) == bytes.size());

    StateValue val;
    bool first = true;
    IntType i8Ty("", 8);

    for (auto &b: bytes) {
      StateValue v(b.nonptr_value(),
                   i8Ty.combine_poison(!b.is_ptr(), b.nonptr_nonpoison()));
      val = first ? move(v) : v.concat(val);
      first = false;
    }
    return toType.fromBV(val.trunc(bitsize));
  }
}
}


namespace IR {

Pointer::Pointer(const Memory &m, const char *var_name)
  : m(m), p(expr::mkVar(var_name, total_bits())) {}

Pointer::Pointer(const Memory &m, unsigned bid, bool local) : m(m) {
  expr bid_expr;
  if (local)
    bid_expr = expr::mkUInt((uint64_t)bid << m.bits_for_nonlocal_bid,
                            m.bitsBid());
  else
    bid_expr = expr::mkUInt(bid, m.bitsBid());
  p = expr::mkUInt(0, m.bits_for_offset).concat(bid_expr);
}

Pointer::Pointer(const Memory &m, const expr &offset, const expr &local_bid,
                 const expr &nonlocal_bid)
  : m(m), p(offset.concat(local_bid).concat(nonlocal_bid)) {}

unsigned Pointer::total_bits() const {
  return m.bitsBid() + m.bits_for_offset;
}

expr Pointer::is_local() const {
  // bid == 0 is the global null block
  return get_local_bid() != 0;
}

expr Pointer::get_bid() const {
  return p.extract(m.bitsBid() - 1, 0);
}

expr Pointer::get_local_bid() const {
  return p.extract(m.bitsBid() - 1, m.bits_for_nonlocal_bid);
}

expr Pointer::get_nonlocal_bid() const {
  return p.extract(m.bits_for_nonlocal_bid - 1, 0);
}

expr Pointer::get_offset() const {
  return p.extract(m.bitsBid() + m.bits_for_offset - 1, m.bitsBid());
}

expr Pointer::get_address() const {
  expr offset = get_offset().sextOrTrunc(m.bits_size_t);
  return offset + m.blocks_addr.load(get_bid());
}

expr Pointer::block_size() const {
  return
    expr::mkUInt(0, 1).concat(
      expr::mkIf(is_local(),
                 m.blocks_size_local.load(get_local_bid()),
                 m.mk_size_array(false).load(get_nonlocal_bid())));
}

Pointer Pointer::operator+(const expr &bytes) const {
  expr off = (get_offset().sextOrTrunc(m.bits_size_t) +
              bytes.zextOrTrunc(m.bits_size_t)).trunc(m.bits_for_offset);
  return { m, off.concat(get_bid()) };
}

Pointer Pointer::operator+(unsigned bytes) const {
  return *this + expr::mkUInt(bytes, m.bits_for_offset);
}

void Pointer::operator+=(const expr &bytes) {
  p = (*this + bytes).p;
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

// When bytes is 0, pointer is always derefenceable
void Pointer::is_dereferenceable(const expr &bytes0, unsigned align,
                                 bool iswrite) {
  expr block_sz = block_size();
  expr offset = get_offset().sextOrTrunc(m.bits_size_t);
  expr bytes = bytes0.zextOrTrunc(m.bits_size_t);

  // 1) check that offset is within bounds and that arith doesn't overflow
  expr cond = (offset + bytes).ule(block_sz);
  cond &= offset.add_no_uoverflow(bytes);

  // 2) check block's address is aligned
  m.state->addUB(is_aligned(align));

  // 3) check block is alive
  cond &= is_block_alive();

  // 4) If it is write, it should not be readonly
  if (iswrite)
    cond &= !is_readonly();

  m.state->addUB(bytes.ugt(0).implies(cond));
}

void Pointer::is_dereferenceable(unsigned bytes, unsigned align, bool iswrite) {
  is_dereferenceable(expr::mkUInt(bytes, m.bits_for_offset), align, iswrite);
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
                  disjoint(get_offset().sextOrTrunc(m.bits_size_t),
                           len1.zextOrTrunc(m.bits_size_t),
                           ptr2.get_offset().sextOrTrunc(m.bits_size_t),
                           len2.zextOrTrunc(m.bits_size_t)));
}

expr Pointer::is_block_alive() const {
  return m.blocks_liveness.load(get_bid());
}

expr Pointer::is_at_heap() const {
  return m.blocks_kind.load(get_bid()) == 1;
}

expr Pointer::is_readonly() const {
  return m.blocks_readonly.load(get_bid());
}

Pointer Pointer::mkNullPointer(const Memory &m) {
  // A null pointer points to block 0.
  return Pointer(m, 0, false);
}

ostream& operator<<(ostream &os, const Pointer &p) {
  if ((p == Pointer::mkNullPointer(p.getMemory())).isTrue())
    return os << "null";

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

expr Memory::mk_val_array() const {
  return expr::mkArray("blks_val", expr::mkUInt(0, bitsBid() + bits_for_offset),
                                   expr::mkUInt(0, bitsByte()));
}

expr Memory::mk_liveness_uf() const {
  auto range = expr::mkUInt(0, bitsBid());
  return expr::mkArray("blks_liveness", range, true);
}

expr Memory::mk_readonly_array() const {
  return expr::mkArray("blks_readonly", expr::mkUInt(0, bitsBid()), false);
}

expr Memory::mk_addr_array() const {
  return expr::mkArray("blks_addr", expr::mkUInt(0, bitsBid()),
                       expr::mkUInt(0, bitsPtrSize()));
}

expr Memory::mk_kind_array() const {
  return expr::mkArray("blks_kind", expr::mkUInt(0, bitsBid()),
                       expr::mkUInt(0, 1));
}

expr Memory::mk_size_array(bool local) const {
  // ASSUMPTION: programs can only allocate up to half of address space
  // so the first bit of size is always zero.
  // We need this assumption to support negative offsets.
  auto range = expr::mkUInt(0,
                            local ? bits_for_local_bid : bits_for_nonlocal_bid);
  auto name = local ? mkName("blks_size") : string("blks_size");
  return expr::mkArray(name.c_str(), range, expr::mkUInt(0, bitsPtrSize() - 1));
}

// last_bid stores 1 + the last memory block id.
// Global block id 0 is reserved for a null block.
// TODO: In the twin memory, there is no null block, so the initial last_bid
// should depend on whether the twin memory is used or not.
// Local bid > 0 since bids == 0 is the global block 0
static unsigned last_local_bid;
static unsigned last_nonlocal_bid;
static unsigned last_idx_ptr;

Memory::Memory(State &state) : state(&state) {
  blocks_val = mk_val_array();
  blocks_readonly = mk_readonly_array();
  blocks_liveness = mk_liveness_uf();
  blocks_addr = mk_addr_array();
  blocks_size_local = mk_size_array(true);
  blocks_kind = mk_kind_array();

  {
    Pointer idx(*this, "#idx0");

    // All non-local blocks cannot initially contain pointers to local blocks.
    auto byte = Byte(*this, blocks_val.load(idx()));
    Pointer loadedptr(*this, byte.ptr_value());
    expr cond = !byte.is_ptr() || !byte.ptr_nonpoison() ||
                !loadedptr.is_local();
    state.addAxiom(expr::mkForAll({ idx() }, move(cond)));
  }

  // Initialize a memory block for null pointer.
  // TODO: in twin memory model, this is not needed.
  auto nullPtr = Pointer::mkNullPointer(*this);
  state.addAxiom(nullPtr.get_address() == 0);
  state.addAxiom(nullPtr.block_size() == 0);

  assert(bits_for_offset <= bits_size_t);
}

void Memory::resetGlobalData() {
  last_local_bid = 1;
  last_nonlocal_bid = 1;
  last_idx_ptr = 1;
}

void Memory::resetLocalBids() {
  last_local_bid = 1;
}

pair<expr, vector<expr>> Memory::mkInput(const char *name) {
  unsigned bits = bits_for_nonlocal_bid + bits_for_offset;
  expr var = expr::mkVar(name, bits);
  expr offset = var.extract(bits - 1, bits_for_nonlocal_bid);
  expr bid = var.extract(bits_for_nonlocal_bid - 1, 0);
  expr local_bid = expr::mkUInt(0, bits_for_local_bid);
  return { Pointer(*this, offset, local_bid, bid).release(), { var } };
}

expr Memory::alloc(const expr &size, unsigned align, BlockKind blockKind,
                   optional<unsigned> bidopt, unsigned *bid_out) {
  // Produce a local block if blockKind is heap or stack.
  bool is_local = blockKind != GLOBAL && blockKind != CONSTGLOBAL;

  auto &last_bid = is_local ? last_local_bid : last_nonlocal_bid;
  unsigned bid = bidopt ? *bidopt : last_bid;
  if (!bidopt)
    ++last_bid;
  assert(bid < last_bid);

  if (bid_out)
    *bid_out = bid;

  Pointer p(*this, bid, is_local);

  if (is_local) {
    expr newaddr = expr::mkFreshVar("addr", expr::mkUInt(0, bitsPtrSize()));
    // A local block's address is quantified because a memory block's address
    // is nondeterminstically chosen, as the result of a freeze instruction.
    // Note that determining whether this can be null or not is a different
    // question, and it is addressed at Malloc::toSMT().
    state->addQuantVar(newaddr);
    blocks_addr = blocks_addr.store(p.get_bid(), newaddr);
  }

  state->addPre(p.block_size() == size.zextOrTrunc(bits_size_t));
  state->addPre(p.is_aligned(align), is_local);
  // TODO: If its address space is not 0, its address can be 0.
  state->addPre(p.get_address() != 0, is_local);
  // TODO: address of new blocks should be disjoint from other live blocks

  if (is_local) {
    // The memory block was initially not allocated.
    state->addPre(!mk_liveness_uf().load(p.get_bid()));
    blocks_liveness = blocks_liveness.store(p.get_bid(), true);

    // initialize its value as non-pointer, poison value
    // This is okay because loading a pointer as non-pointer is also poison.
    expr ofs = expr::mkFreshVar("ofs", expr::mkUInt(0, bitsOffset()));
    state->addPre(expr::mkForAll({ ofs }, mk_val_array().load(ofs.concat(p.get_bid())) == 0));
  } else {
    // The memory block was initially alive.
    state->addAxiom(mk_liveness_uf().load(p.get_bid()));
  }
  blocks_kind = blocks_kind.store(p.get_bid(),
                                  expr::mkUInt(blockKind == HEAP, 1));
  blocks_readonly = blocks_readonly.store(p.get_bid(),
                                          blockKind == CONSTGLOBAL);

  return p.release();
}


void Memory::free(const expr &ptr) {
  Pointer p(*this, ptr);
  auto isNullPointer = p == Pointer::mkNullPointer(*this);

  state->addUB(isNullPointer || ((p.get_offset() == 0) &&
                                 (p.is_block_alive()) &&
                                 (p.is_at_heap())));
  blocks_liveness = blocks_liveness.store(p.get_bid(), false);
}

void Memory::store(const expr &p, const StateValue &v, const Type &type,
                   unsigned align) {
  vector<Byte> bytes = valueToBytes(v, type, *this);

  Pointer ptr(*this, p);
  ptr.is_dereferenceable(bytes.size(), align, true);
  for (unsigned i = 0, e = bytes.size(); i < e; ++i) {
    // FIXME: support big-endian
    blocks_val = blocks_val.store((ptr + i).release(), bytes[i]());
  }
}

StateValue Memory::load(const expr &p, const Type &type, unsigned align) {
  auto bitsize = type.isPtrType() ? bitsPtrSize() : type.bits();
  unsigned bytesize = divide_up(bitsize, 8);

  Pointer ptr(*this, p);
  ptr.is_dereferenceable(bytesize, align, false);

  vector<Byte> loadedBytes;
  for (unsigned i = 0; i < bytesize; ++i) {
    loadedBytes.emplace_back(*this, blocks_val.load((ptr + i).release()));
  }
  return bytesToValue(loadedBytes, type);
}

void Memory::memset(const expr &p, const StateValue &val, const expr &bytesize,
                    unsigned align) {
  assert(val.bits() == 8);
  Pointer ptr(*this, p);
  ptr.is_dereferenceable(bytesize, align, true);

  vector<Byte> bytes = valueToBytes(val, IntType("", 8), *this);
  assert(bytes.size() == 1);

  uint64_t n;
  if (bytesize.isUInt(n) && n <= 4) {
    for (unsigned i = 0; i < n; ++i) {
      auto p = (ptr + i).release();
      blocks_val = blocks_val.store(p, bytes[0]());
    }
  } else {
    string name = "#idx_" + to_string(last_idx_ptr++);
    Pointer idx(*this, name.c_str());

    expr cond = idx.uge(ptr).both() && idx.ult(ptr + bytesize).both();
    expr val = expr::mkIf(cond, bytes[0](), blocks_val.load(idx()));
    blocks_val = expr::mkLambda({ idx() }, move(val));
  }
}

void Memory::memcpy(const expr &d, const expr &s, const expr &bytesize,
                    unsigned align_dst, unsigned align_src, bool is_move) {
  Pointer dst(*this, d), src(*this, s);
  dst.is_dereferenceable(bytesize, align_dst, true);
  src.is_dereferenceable(bytesize, align_src, false);
  if (!is_move)
    src.is_disjoint(bytesize, dst, bytesize);

  uint64_t n;
  if (bytesize.isUInt(n) && n <= 4) {
    auto old_val = blocks_val;
    for (unsigned i = 0; i < n; ++i) {
      auto src_i = (src + i).release();
      auto dst_i = (dst + i).release();
      blocks_val = blocks_val.store(dst_i, old_val.load(src_i));
    }
  } else {
    string name = "#idx_" + to_string(last_idx_ptr++);
    Pointer dst_idx(*this, expr::mkVar(name.c_str(), dst.bits()));
    Pointer src_idx = src + (dst_idx.get_offset() - dst.get_offset());

    expr cond = dst_idx.uge(dst).both() && dst_idx.ult(dst + bytesize).both();
    expr val = expr::mkIf(cond, blocks_val.load(src_idx()),
                          blocks_val.load(dst_idx()));
    blocks_val = expr::mkLambda({ dst_idx() }, move(val));
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
  ret.blocks_val      = expr::mkIf(cond, then.blocks_val, els.blocks_val);
  ret.blocks_liveness = expr::mkIf(cond, then.blocks_liveness,
                                   els.blocks_liveness);
  ret.blocks_kind     = expr::mkIf(cond, then.blocks_kind, els.blocks_kind);
  return ret;
}

}
