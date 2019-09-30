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
void Pointer::is_dereferenceable(const expr &bytes0, unsigned align) {
  expr block_sz = block_size();
  expr offset = get_offset().sextOrTrunc(m.bits_size_t);
  expr bytes = bytes0.zextOrTrunc(m.bits_size_t);

  // 1) check that offset is within bounds and that arith doesn't overflow
  expr cond = (offset + bytes).ule(block_sz);
  cond &= offset.add_no_uoverflow(bytes);

  // 2) check block's address is aligned
  cond &= is_aligned(align);

  // 3) check block is alive
  cond &= is_block_alive();

  m.state->addUB(bytes.ugt(0).implies(cond));
}

void Pointer::is_dereferenceable(unsigned bytes, unsigned align) {
  is_dereferenceable(expr::mkUInt(bytes, m.bits_for_offset), align);
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

Pointer Pointer::mkNullPointer(Memory &m) {
  // A null pointer points to block 0.
  return Pointer(m, 0, false);
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
                             expr::mkUInt(0, 2 * 8)); // val + poison bits
}

expr Memory::mk_liveness_uf() const {
  unsigned bits_bids = bits_for_local_bid + bits_for_nonlocal_bid;
  return expr::mkArray("blks_liveness", expr::mkUInt(0, bits_bids), true);
}

Memory::Memory(State &state) : state(&state) {
  blocks_val = mk_val_array("blks_val");
  {
    // initialize all local blocks as poison
    Pointer idx(*this, "#idx0");
    expr poison = expr::mkUInt(0, 16);
    expr val = expr::mkIf(idx.is_local(), poison, blocks_val.load(idx()));
    blocks_val = expr::mkLambda({ idx() }, move(val));
  }

  blocks_liveness = mk_liveness_uf();

  unsigned bits_bids = bits_for_local_bid + bits_for_nonlocal_bid;
  blocks_kind = expr::mkArray("blks_kind", expr::mkUInt(0, bits_bids),
                              expr::mkUInt(0, 1));

  // Initialize a memory block for null pointer.
  // TODO: in twin memory model, this is not needed.
  auto nullPtr = Pointer::mkNullPointer(*this);
  state.addPre(nullPtr.get_address() == 0);
  state.addPre(nullPtr.block_size() == 0);

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

expr Memory::alloc(const expr &size, unsigned align, BlockKind blockKind,
                   optional<unsigned> bidopt, unsigned *bid_out) {
  unsigned bid = bidopt ? *bidopt : last_bid;
  if (!bidopt)
    last_bid++;
  assert(bid < last_bid);

  if (bid_out)
    *bid_out = bid;

  // It is allocated as a local block if blockKind is heap or stack.
  bool is_local = blockKind != GLOBAL;

  Pointer p(*this, bid, is_local);
  state->addPre(p.is_aligned(align));

  expr size_zextOrTrunced = size.zextOrTrunc(bits_size_t);
  state->addPre(p.block_size() == size_zextOrTrunced);
  // TODO: If its address space is not 0, its address can be 0.
  state->addPre(p.get_address() != 0);
  // TODO: address of new blocks should be disjoint from other live blocks

  if (is_local) {
    // Initially there was no such block, now it is allocated.
    state->addPre(!mk_liveness_uf().load(p.get_bid()));
    blocks_liveness = blocks_liveness.store(p.get_bid(), true);
  } else {
    // The memory block was initially alive.
    // TODO: const global variables should be read-only
    state->addPre(mk_liveness_uf().load(p.get_bid()));
  }
  blocks_kind = blocks_kind.store(p.get_bid(),
                                  expr::mkUInt(blockKind == HEAP, 1));

  return p();
}


void Memory::free(const expr &ptr) {
  Pointer p(*this, ptr);
  auto isNullPointer = p == Pointer::mkNullPointer(*this);

  state->addUB(isNullPointer || ((p.get_offset() == 0) &&
                                 (p.is_block_alive()) &&
                                 (p.is_at_heap())));
  blocks_liveness = blocks_liveness.store(p.get_bid(), false);
}

void Memory::store(const expr &p, const StateValue &v, Type &type,
                   unsigned align) {
  StateValue val = type.toBV(v);
  unsigned bits = val.bits();
  unsigned bytes = divide_up(bits, 8);

  val = val.zext(bytes * 8 - bits);

  Pointer ptr(*this, p);
  ptr.is_dereferenceable(bytes, align);

  for (unsigned i = 0; i < bytes; ++i) {
    // FIXME: right now we store in little-endian; consider others?
    expr data  = val.value.extract((i + 1) * 8 - 1, i * 8);
    expr np    = val.non_poison.extract((i + 1) * 8 - 1, i * 8);
    blocks_val = blocks_val.store((ptr + i).release(), np.concat(data));
  }
}

StateValue Memory::load(const expr &p, Type &type, unsigned align) {
  auto bits = type.bits();
  unsigned bytes = divide_up(bits, 8);
  Pointer ptr(*this, p);
  ptr.is_dereferenceable(bytes, align);

  StateValue val;
  bool first = true;

  for (unsigned i = 0; i < bytes; ++i) {
    auto ptr_i = (ptr + i).release();
    expr pair = blocks_val.load(ptr_i);
    StateValue v(pair.extract(7, 0),  pair.extract(15, 8));
    val = first ? move(v) : v.concat(val);
    first = false;
  }

  return type.fromBV(val.trunc(bits));
}

void Memory::memset(const expr &p, const StateValue &val, const expr &bytes,
                    unsigned align) {
  assert(val.bits() == 8);
  Pointer ptr(*this, p);
  ptr.is_dereferenceable(bytes, align);
  auto valbv = IntType("", 8).toBV(val);
  expr store_val = valbv.non_poison.concat(valbv.value);

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
                    unsigned align_dst, unsigned align_src, bool is_move) {
  Pointer dst(*this, d), src(*this, s);
  dst.is_dereferenceable(bytes, align_dst);
  src.is_dereferenceable(bytes, align_src);
  if (!is_move)
    src.is_disjoint(bytes, dst, bytes);

  uint64_t n;
  if (bytes.isUInt(n) && n <= 4) {
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

    expr cond = dst_idx.uge(dst).both() && dst_idx.ult(dst + bytes).both();
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
  ret.blocks_val   = expr::mkIf(cond, then.blocks_val, els.blocks_val);
  ret.blocks_liveness = expr::mkIf(cond, then.blocks_liveness,
                                   els.blocks_liveness);
  ret.blocks_kind = expr::mkIf(cond, then.blocks_kind, els.blocks_kind);
  // FIXME: this isn't correct; should be a per function counter
  ret.last_bid     = max(then.last_bid, els.last_bid);
  ret.last_idx_ptr = max(then.last_idx_ptr, els.last_idx_ptr);
  return ret;
}

void Memory::bumpLastBid(unsigned bid) {
  assert(last_bid <= bid + 1);
  last_bid = bid + 1;
}

}
