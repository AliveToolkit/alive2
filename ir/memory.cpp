// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/memory.h"
#include "ir/state.h"
#include "util/compiler.h"

using namespace smt;
using namespace std;
using namespace util;

namespace IR {

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
  // Memory, which is needed to get the size of offset and block id.
  const Memory &m;
  // The underlying representation.
  smt::expr p;

public:
  Byte(Byte &&b): m(b.m), p(std::move(b.p)) {}
  // Creates a byte with its raw representation.
  Byte(const Memory &m, smt::expr &&byterepr);
  // Creates a pointer byte that represents i'th byte of p.
  // non_poison should be an one-bit vector or boolean.
  Byte(const Pointer &p, unsigned i, const smt::expr &non_poison);
  // Creates a non-pointer byte that has data and non_poison.
  // data and non_poison should have 8 bits.
  Byte(const Memory &m, const smt::expr &data, const smt::expr &non_poison);

  // Returns a bit that represents whether this byte is a pointer byte.
  smt::expr is_ptrbyte() const { return p.extract(p.bits() - 1, p.bits() - 1); }
  // Assuming that this is a pointer byte, returns a single bit that represents
  // whether the byte is poison.
  smt::expr ptr_nonpoison() const {
    return p.extract(p.bits() - 2, p.bits() - 2);
  }
  // Returns its value assuming that this is a pointer byte.
  smt::expr ptr_value() const { return p.extract(p.bits() - 3, 3); }
  // Assuming that this is a pointer byte, returns its byte offset.
  smt::expr ptr_byteoffset() const { return p.extract(2, 0); }
  // Assuming that this is a non-pointer byte, returns 8-bit bitvector.
  smt::expr nonptr_nonpoison() const { return p.extract(15, 8); };
  // Assuming that this is a non-pointer byte, returns its value.
  smt::expr nonptr_value() const { return p.extract(7, 0); }

  const smt::expr& operator()() const { return p; }
  smt::expr release() const { return std::move(p); }
};


Pointer::Pointer(const Memory &m, const char *var_name)
  : m(m), p(expr::mkVar(var_name, total_bits())) {}

Pointer::Pointer(const Memory &m, unsigned bid, bool local) : m(m) {
  expr bid_expr;
  if (local)
    bid_expr = expr::mkUInt((uint64_t)bid << m.bits_for_nonlocal_bid,
                            bits_for_bids());
  else
    bid_expr = expr::mkUInt(bid, bits_for_bids());
  p = expr::mkUInt(0, m.bits_for_offset).concat(bid_expr);
}

Pointer::Pointer(const Memory &m, const expr &offset, const expr &local_bid,
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
  m.state->addUB(is_aligned(align));

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


Byte::Byte(const Memory &m, expr &&byterepr): m(m), p(move(byterepr)) {
  assert(p.bits() == m.bitsByte());
}

Byte::Byte(const Pointer &ptr, unsigned i, const expr &non_poison)
    : m(ptr.getMemory()) {
  // TODO: support pointers larger than 64 bits.
  assert(m.bitsPtrSize() <= 64 && m.bitsPtrSize() % 8 == 0);

  expr byteofs = expr::mkUInt(i, 3);
  p = expr::mkUInt(1, 1).concat(non_poison.isBool() ? non_poison.toBVBool() :
                                non_poison).concat(ptr()).concat(byteofs);
  assert(p.bits() == m.bitsByte());
}

Byte::Byte(const Memory &m, const expr &data, const expr &non_poison): m(m) {
  assert(m.bitsByte() >= 1 + 8 + 8);
  assert(data.bits() == 8);
  assert(non_poison.bits() == 8);

  unsigned padding = m.bitsByte() - 1 - 8 - 8;
  p = expr::mkUInt(0, 1).concat(expr::mkUInt(0, padding)).concat(non_poison)
                        .concat(data);
}


// Converts StateValue to Byte array.
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

// Converts bytes to StateValue.
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

    for (unsigned i = 0; i < bytes.size(); ++i) {
      auto &b = bytes[i];
      expr is_ptrbyte = b.is_ptrbyte();
      expr byte_ofs = b.ptr_byteoffset();
      expr ptr_value = b.ptr_value();
      expr is_nonpoisonb = b.ptr_nonpoison();

      if (first) {
        loaded_ptr = move(ptr_value);
      } else {
        non_poison &= ptr_value == loaded_ptr;
      }
      non_poison &= is_ptrbyte == 1;
      non_poison &= is_nonpoisonb == 1;
      non_poison &= byte_ofs == i;
    }
    return { move(loaded_ptr), move(non_poison) };

  } else {
    auto bitsize = toType.bits();
    unsigned bytesize = divide_up(bitsize, 8);
    assert(bytesize == bytes.size());

    StateValue val;
    bool first = true;
    // The result is poison if any byte is a pointer byte.
    expr non_poison = true;

    for (auto &b: bytes) {
      StateValue v(b.nonptr_value(), b.nonptr_nonpoison());

      val = first ? move(v) : v.concat(val);
      first = false;
      non_poison &= b.is_ptrbyte() == 0;
    }

    val = toType.fromBV(val.trunc(bitsize));
    val.non_poison &= non_poison;
    return val;
  }
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
                             expr::mkUInt(0, bitsByte()));
}

expr Memory::mk_liveness_uf() const {
  unsigned bits_bids = bits_for_local_bid + bits_for_nonlocal_bid;
  return expr::mkArray("blks_liveness", expr::mkUInt(0, bits_bids), true);
}

Memory::Memory(State &state) : state(&state) {
  blocks_val = mk_val_array("blks_val");
  {
    // initialize all local blocks as non-pointer, poison value
    // This is okay because loading a pointer as non-pointer is also poison.
    Pointer idx(*this, "#idx0");
    expr poison = expr::mkUInt(0, bitsByte());
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

void Memory::store(const expr &p, const StateValue &v, const Type &type,
                   unsigned align) {
  vector<Byte> bytes = valueToBytes(v, type, *this);

  Pointer ptr(*this, p);
  ptr.is_dereferenceable(bytes.size(), align);
  for (unsigned i = 0, e = bytes.size(); i < e; ++i) {
    // FIXME: support big-endian
    blocks_val = blocks_val.store((ptr + i).release(), bytes[i]());
  }
}

StateValue Memory::load(const expr &p, const Type &type, unsigned align) {
  auto bitsize = type.isPtrType() ? bitsPtrSize() : type.bits();
  unsigned bytesize = divide_up(bitsize, 8);

  Pointer ptr(*this, p);
  ptr.is_dereferenceable(bytesize, align);

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
  ptr.is_dereferenceable(bytesize, align);

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
  dst.is_dereferenceable(bytesize, align_dst);
  src.is_dereferenceable(bytesize, align_src);
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
