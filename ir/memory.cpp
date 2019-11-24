// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/memory.h"
#include "ir/globals.h"
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
    assert(!p.isValid() || p.bits() == m.bitsByte());
  }

  // Creates a pointer byte that represents i'th byte of p.
  // non_poison should be an one-bit vector or boolean.
  Byte(const Pointer &ptr, unsigned i, const expr &non_poison)
    : m(ptr.getMemory()) {
    // TODO: support pointers larger than 64 bits.
    assert(bits_size_t <= 64 && bits_size_t % 8 == 0);

    expr byteofs = expr::mkUInt(i, 3);
    expr np = non_poison.toBVBool();
    p = expr::mkUInt(1, 1).concat(np).concat(ptr()).concat(byteofs);
    assert(!p.isValid() || p.bits() == m.bitsByte());
  }

  // Creates a non-pointer byte that has data and non_poison.
  // data and non_poison should have 8 bits.
  Byte(const Memory &m, const expr &data, const expr &non_poison) : m(m) {
    assert(m.bitsByte() > 1 + 8 + 8);
    assert(!data.isValid() || data.bits() == 8);
    assert(!non_poison.isValid() || non_poison.bits() == 8);

    unsigned padding = m.bitsByte() - 1 - 8 - 8;
    p = expr::mkUInt(0, 1).concat(expr::mkUInt(0, padding)).concat(non_poison)
                          .concat(data);
    assert(!p.isValid() || p.bits() == m.bitsByte());
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

  expr operator==(const Byte &rhs) const {
    assert(&m == &rhs.m);
    return p == rhs.p;
  }

  static Byte mkPoisonByte(const Memory &m) {
    IntType ty("", 8);
    auto v = ty.toBV(ty.getDummyValue(false));
    return { m, v.value, v.non_poison };
  }
};


vector<Byte> valueToBytes(const StateValue &val, const Type &fromType,
                          const Memory &mem) {
  vector<Byte> bytes;
  if (fromType.isPtrType()) {
    Pointer p(mem, val.value);
    unsigned bytesize = bits_size_t / 8;

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
    expr loaded_ptr;
    // The result is not poison if all of these hold:
    // (1) There's no poison byte, and they are all pointer bytes
    // (2) All of the bytes have the same information
    // (3) Byte offsets should be correct
    expr non_poison = true;

    for (unsigned i = 0, e = bytes.size(); i < e; ++i) {
      auto &b = bytes[i];
      expr ptr_value = b.ptr_value();

      if (i == 0) {
        loaded_ptr = move(ptr_value);
      } else {
        non_poison &= ptr_value == loaded_ptr;
      }
      non_poison &= b.is_ptr();
      non_poison &= b.ptr_nonpoison();
      non_poison &= b.ptr_byteoffset() == i;
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

static bool observes_addresses() {
  return IR::has_ptr2int || IR::has_int2ptr;
}


namespace IR {

Pointer::Pointer(const Memory &m, const char *var_name)
  : m(m), p(expr::mkUInt(0, 1).concat(
                expr::mkFreshVar(var_name, expr::mkUInt(0, total_bits()-1)))) {}

Pointer::Pointer(const Memory &m, unsigned bid, bool local) : m(m) {
  expr bid_expr = expr::mkUInt(bid, bits_for_bid - 1);
  p = expr::mkUInt(local, 1).concat(bid_expr)
                            .concat(expr::mkUInt(0, bits_for_offset));
}

Pointer::Pointer(const Memory &m, const expr &bid, const expr &offset)
  : m(m), p(bid.concat(offset)) {}

unsigned Pointer::total_bits() const {
  return bits_for_bid + bits_for_offset;
}

expr Pointer::is_local() const {
  auto bit = total_bits() - 1;
  return p.extract(bit, bit) == 1;
}

expr Pointer::get_bid() const {
  return p.extract(total_bits() - 1, bits_for_offset);
}

expr Pointer::get_short_bid() const {
  return p.extract(total_bits() - 2, bits_for_offset);
}

expr Pointer::get_offset() const {
  return p.extract(bits_for_offset - 1, 0).sextOrTrunc(bits_size_t);
}

expr Pointer::get_value(const char *name, const FunctionExpr &fn,
                        const expr &ret_type) const {
  auto bid = get_short_bid();
  return expr::mkIf(is_local(), fn(bid), expr::mkUF(name, { bid }, ret_type));
}

expr Pointer::get_address(bool simplify) const {
  expr offset = get_offset();

  // fast path for null ptrs
  if (simplify && (get_bid() == 0).isTrue())
    return offset;

  auto bid = get_short_bid();
  // Local block area is the upper half of the memory
  // Non-local block area is the lower half
  auto retty = expr::mkUInt(0, bits_for_offset - 1);
  return offset + expr::mkIf(is_local(),
      expr::mkUInt(1, 1).concat(m.local_blk_addr(bid)),
      expr::mkUInt(0, 1).concat(expr::mkUF("blk_addr", { bid }, retty)));
}

expr Pointer::block_size(bool simplify) const {
  // ASSUMPTION: programs can only allocate up to half of address space
  // so the first bit of size is always zero.
  // We need this assumption to support negative offsets.

  // fast path for null ptrs
  if (simplify && (get_bid() == 0).isTrue())
    return expr::mkUInt(0, bits_size_t);

  expr range = expr::mkUInt(0, bits_size_t - 1);
  return expr::mkUInt(0, 1)
           .concat(get_value("blk_size", m.local_blk_size, range));
}

expr Pointer::short_ptr() const {
  return p.extract(total_bits() - 2, 0);
}

Pointer Pointer::operator+(const expr &bytes) const {
  expr off = (get_offset() + bytes.zextOrTrunc(bits_size_t));
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
  return p == rhs.p;
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
  return get_offset().ule(block_size());
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
  expr offset = get_offset();
  expr bytes = bytes0.zextOrTrunc(bits_size_t);

  // check that offset is within bounds and that arith doesn't overflow
  expr cond = (offset + bytes).ule(block_sz);
  cond &= offset.add_no_uoverflow(bytes);

  cond &= is_block_alive();

  if (iswrite)
    cond &= !is_readonly();

  m.state->addUB(bytes.uge(1).implies(cond));

  // address must be always aligned regardless of access size
  m.state->addUB(is_aligned(align));
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
  auto bid = get_short_bid();
  return expr::mkIf(is_local(), m.local_block_liveness.load(bid),
                    m.non_local_block_liveness.load(bid));
}

expr Pointer::get_alloc_type() const {
  return get_value("blk_kind", m.local_blk_kind, expr::mkUInt(0, 2));
}

expr Pointer::is_heap_allocated() const {
  return get_alloc_type() != NON_HEAP;
}

expr Pointer::refined(const Pointer &other) const {
  // This refers to a block that was malloc'ed within the function
  expr local = other.is_heap_allocated();
  local &= other.is_block_alive();
  local &= other.block_size() == block_size();
  // TODO: this induces an infinite loop; need a quantifier or rec function
  // TODO: needs variable offset
  //local &= block_refined(other);

  local = (is_heap_allocated() && is_block_alive()).implies(local);

  return expr::mkIf(is_local(), local, *this == other);
}

expr Pointer::block_refined(const Pointer &other) const {
  if (m.non_local_block_val.eq(other.m.non_local_block_val) &&
      m.non_local_block_liveness.eq(other.m.non_local_block_liveness))
    return true;

  expr liveness = is_block_alive() == other.is_block_alive();

  Byte val(m, m.non_local_block_val.load(short_ptr()));
  Byte val2(other.m, other.m.non_local_block_val.load(other.short_ptr()));

  expr check = is_block_alive() && get_offset().ult(block_size());

  // TODO: do we need to type memory and do a type-specific refinement check?
  expr np1 = val.nonptr_nonpoison();
  expr int_cnstr = (val2.nonptr_nonpoison() | np1) == np1 &&
                   (val.nonptr_value() | np1) == (val2.nonptr_value() | np1);

  // fast path: if we didn't do any ptr store, then all ptrs in memory were
  // already there and don't need checking
  if (!m.did_pointer_store && !other.m.did_pointer_store)
    return liveness &&
           check.implies(expr::mkIf(val.is_ptr(), val == val2, int_cnstr));

  Pointer load_ptr(m, val.ptr_value());
  Pointer load_ptr2(other.m, val2.ptr_value());
  expr ptr_cnstr = val.ptr_nonpoison().implies(load_ptr.refined(load_ptr2));

  return liveness &&
         check.implies(expr::mkIf(val.is_ptr(), ptr_cnstr, int_cnstr));
}

expr Pointer::is_readonly() const {
  return !is_local() && expr::mkUF("blk_readonly", { get_short_bid() }, false);
}

Pointer Pointer::mkNullPointer(const Memory &m) {
  // A null pointer points to block 0.
  return { m, 0, false };
}

expr Pointer::isNull() const {
  return *this == mkNullPointer(m);
}

ostream& operator<<(ostream &os, const Pointer &p) {
  if (p.isNull().isTrue())
    return os << "null";

  os << "pointer(" << (p.is_local().isTrue() ? "local" : "non-local")
     << ", block_id=";
  p.get_bid().printUnsigned(os);
  os << ", offset=";
  p.get_offset().printSigned(os);
  return os << ')';
}


void Memory::store(const Pointer &p, const expr &val, expr &local,
                   expr &non_local, bool index_bid) {
  // check if we are potentially storing a pointer
  if (!index_bid)
    did_pointer_store |= !Byte(*this, expr(val)).is_ptr().isFalse();

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

expr Memory::mk_val_array() const {
  return expr::mkArray("blk_val",
                       expr::mkUInt(0, bits_for_bid - 1 + bits_for_offset),
                       expr::mkUInt(0, bitsByte()));
}

expr Memory::mk_liveness_array() const {
  return expr::mkArray("blk_liveness", expr::mkUInt(0, bits_for_bid - 1), true);
}

// last_bid stores 1 + the last memory block id.
// Global block id 0 is reserved for a null block.
static unsigned last_local_bid = 0;
static unsigned last_nonlocal_bid = 1;

static bool memory_unused() {
  // +1 for the null block
  return num_locals == 0 && num_nonlocals == 1 && !nullptr_is_used;
}

Memory::Memory(State &state)
  : state(&state),
    local_blk_addr(expr::mkUInt(0, bits_size_t - 1)),
    local_blk_size(expr::mkUInt(0, bits_size_t - 1)),
    local_blk_kind(expr::mkUInt(0, 2)) {

  if (memory_unused())
    return;

  non_local_block_val = mk_val_array();
  non_local_block_liveness = mk_liveness_array();
  {
    Pointer ptr(*this, "#idx");
    auto idx = ptr.short_ptr();

    if (state.isSource()) {
      // All non-local blocks cannot initially contain pointers to local blocks.
      auto byte = Byte(*this, non_local_block_val.load(idx));
      Pointer loadedptr(*this, byte.ptr_value());
      expr cond = (byte.is_ptr() && byte.ptr_nonpoison())
                    .implies(!loadedptr.is_local());
      state.addAxiom(expr::mkForAll({ idx }, move(cond)));
    }

    // initialize all local blocks as non-pointer, poison value
    // This is okay because loading a pointer as non-pointer is also poison.
    local_block_val = expr::mkLambda({ idx }, Byte::mkPoisonByte(*this)());
  }

  // all local blocks are dead in the beginning
  local_block_liveness
    = expr::mkLambda({ expr::mkVar("#bid0", bits_for_bid - 1) }, false);

  // A memory space is separated into non-local area / local area.
  // Non-local area is the lower half of memory (to include null pointer),
  // local area is the upper half.
  // This helps efficient encoding of disjointness between local and nonlocal
  // blocks.
  // The last byte of the memory cannot be allocated because ptr + size
  // (where size is the size of the block and ptr is the beginning address of
  // the block) should not overflow.

  // The initially available space of local area. Src and tgt shares this
  // value.
  local_avail_space = expr::mkVar("local_avail_space", bits_size_t - 1);

  if (state.isSource()) {
    auto nullp = Pointer::mkNullPointer(*this);
    state.addAxiom(nullp.block_size(false) == 0);
    state.addAxiom(!nullp.is_block_alive());
  }

  if (state.isSource() && observes_addresses()) {
    // Initialize a memory block for null pointer.
    // TODO: in twin memory model, this is not needed.
    state.addAxiom(Pointer::mkNullPointer(*this).get_address(false) == 0);

    // Non-local blocks are disjoint.
    // Ignore null pointer block
    unsigned non_local_bid_upperbound = 1 << (bits_for_bid - 1);
    for (unsigned bid = 1; bid < non_local_bid_upperbound; ++bid) {
      Pointer p1(*this, bid, false);
      expr disj = p1.get_address() != 0;

      for (unsigned bid2 = bid + 1; bid2 < non_local_bid_upperbound; ++bid2) {
        Pointer p2(*this, bid2, false);
        disj &= p2.is_block_alive()
                    .implies(disjoint(p1.get_address(), p1.block_size(),
                                      p2.get_address(), p2.block_size()));
      }
      state.addAxiom(p1.is_block_alive().implies(disj));

      // Ensure block doesn't spill to local memory
      auto bit = bits_size_t - 1;
      state.addAxiom((p1.get_address() + p1.block_size())
                       .extract(bit, bit) == 0);
    }
  }

  // ensure globals fit in their reserved space
  if (state.isSource()) {
    auto sum_globals = expr::mkUInt(0, bits_size_t - 1);

    unsigned non_local_bid_upperbound = 1 << (bits_for_bid - 1);
    for (unsigned bid = 1; bid < non_local_bid_upperbound; ++bid) {
      Pointer p(*this, bid, false);
      auto sz = p.block_size().extract(bits_size_t - 2, 0);
      state.addAxiom(p.is_block_alive()
                      .implies(sum_globals.add_no_uoverflow(sz)));
      sum_globals = expr::mkIf(p.is_block_alive(), sum_globals + sz,
                               sum_globals);
    }
  }

  assert(bits_for_offset <= bits_size_t);
}

void Memory::resetGlobalData() {
  resetLocalBids();
  last_nonlocal_bid = 1;
}

void Memory::resetLocalBids() {
  last_local_bid = 0;
}

unsigned Memory::bitsByte() const {
  return 1 + 1 + bits_for_bid + bits_for_offset + 3;
}

expr Memory::mkInput(const char *name) const {
  unsigned bits = bits_for_bid - 1 + bits_for_offset;
  expr var = expr::mkVar(name, bits);
  expr offset = var.extract(bits_for_offset - 1, 0);
  expr bid = var.extract(bits - 1, bits_for_offset);
  expr is_local = expr::mkUInt(0, 1);
  return Pointer(*this, is_local.concat(bid), offset).release();
}

pair<expr, expr> Memory::mkUndefInput() const {
  expr offset = expr::mkFreshVar("undef", expr::mkUInt(0, bits_for_offset));
  Pointer p(*this, expr::mkUInt(0, bits_for_bid), offset);
  return { p.release(), move(offset) };
}

expr Memory::alloc(const expr &size, unsigned align, BlockKind blockKind,
                   optional<unsigned> bidopt, unsigned *bid_out,
                   const expr &precond) {
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
  expr allocated = size_zext.extract(bits_size_t - 1, bits_size_t - 1) == 0;

  allocated &= precond;

  Pointer p(*this, bid, is_local);
  auto short_bid = p.get_short_bid();
  // TODO: If address space is not 0, the address can be 0.
  // TODO: add support for C++ allocs

  unsigned alloc_ty = blockKind == HEAP ? Pointer::MALLOC : Pointer::NON_HEAP;

  if (is_local) {
    assert(align != 0);
    auto align_bits = ilog2(align);

    // MSB of local block area's address is 1.
    auto addr_var
      = expr::mkFreshVar("local_addr",
                         expr::mkUInt(0, bits_size_t - align_bits - 1));
    state->addQuantVar(addr_var);
    auto blk_addr = align_bits ? addr_var.concat(expr::mkUInt(0, align_bits))
                               : addr_var;

    auto size_upperbound = size_zext + expr::mkUInt(align - 1, bits_size_t);
    allocated &= size_upperbound.ule(local_avail_space.zext(1));

    // Disjointness of block's address range with other local blocks
    if (observes_addresses()) {
      auto zero = expr::mkUInt(0, bits_for_offset);
      auto one = expr::mkUInt(1, 1);
      expr disj(true);
      for (auto &[sbid, addr] : local_blk_addr) {
        (void)addr;
        Pointer p2(*this, one.concat(sbid), zero);
        disj &= p2.is_block_alive()
            .implies(disjoint(one.concat(blk_addr), size_zext, p2.get_address(),
                              p2.block_size()));
      }
      state->addPre(allocated.implies(disj));
    }

    local_blk_addr.add(expr(short_bid), move(blk_addr));
    local_blk_size.add(expr(short_bid), size_zext.trunc(bits_size_t - 1));
    local_blk_kind.add(expr(short_bid), expr::mkUInt(alloc_ty, 2));

    local_block_liveness = local_block_liveness.store(short_bid, allocated);

    // size_upperbound should be subtracted here (not size_zext0) because we
    // need to consider blocks' alignment.
    auto size_upperbound_trunc = size_upperbound.trunc(bits_size_t - 1);
    local_avail_space = expr::mkIf(allocated,
                                   local_avail_space - size_upperbound_trunc,
                                   local_avail_space);

    // addr + size does not overflow.
    if (observes_addresses())
      state->addPre(p.get_address().add_no_uoverflow(p.block_size()));

  } else {
    state->addAxiom(blockKind == CONSTGLOBAL ? p.is_readonly()
                                             : !p.is_readonly());
    // The memory block was initially alive.
    state->addAxiom(allocated.implies(mk_liveness_array().load(short_bid) &&
                                      p.is_aligned(align) &&
                                      p.block_size() == size_zext));
    state->addAxiom(p.get_alloc_type() == alloc_ty);
  }
  return expr::mkIf(allocated, p(), Pointer::mkNullPointer(*this)());
}

void Memory::free(const expr &ptr) {
  assert(!memory_unused());
  Pointer p(*this, ptr);
  state->addUB(p.isNull() || (p.get_offset() == 0 &&
                              p.is_block_alive() &&
                              p.get_alloc_type() == Pointer::MALLOC));
  store(p, false, local_block_liveness, non_local_block_liveness, true);

  // optimization: if this is a local block, remove all associated information
  if (p.is_local().isTrue() && p.get_short_bid().isConst()) {
    expr bid = p.get_short_bid();
    local_blk_addr.del(bid);
    local_blk_size.del(bid);
    local_blk_kind.del(bid);
  }
}

void Memory::store(const expr &p, const StateValue &v, const Type &type,
                   unsigned align) {
  assert(!memory_unused());
  vector<Byte> bytes = valueToBytes(v, type, *this);

  Pointer ptr(*this, p);
  ptr.is_dereferenceable(bytes.size(), align, true);
  for (unsigned i = 0, e = bytes.size(); i < e; ++i) {
    auto ptr_i = little_endian ? (ptr + i) : (ptr + (e - i - 1));
    store(ptr_i, bytes[i](), local_block_val, non_local_block_val);
  }
}

StateValue Memory::load(const expr &p, const Type &type, unsigned align) {
  assert(!memory_unused());
  auto bitsize = type.isPtrType() ? bits_size_t : type.bits();
  unsigned bytesize = divide_up(bitsize, 8);

  Pointer ptr(*this, p);
  ptr.is_dereferenceable(bytesize, align, false);

  vector<Byte> loadedBytes;
  for (unsigned i = 0; i < bytesize; ++i) {
    auto ptr_i = little_endian ? (ptr + i) : (ptr + (bytesize - i - 1));
    loadedBytes.emplace_back(*this, ::load(ptr_i, local_block_val,
                                           non_local_block_val));
  }
  return bytesToValue(loadedBytes, type);
}

void Memory::memset(const expr &p, const StateValue &val, const expr &bytesize,
                    unsigned align) {
  assert(!memory_unused());
  assert(!val.isValid() || val.bits() == 8);
  Pointer ptr(*this, p);
  ptr.is_dereferenceable(bytesize, align, true);

  vector<Byte> bytes = valueToBytes(val, IntType("", 8), *this);
  assert(bytes.size() == 1);

  uint64_t n;
  if (bytesize.isUInt(n) && n <= 4) {
    for (unsigned i = 0; i < n; ++i) {
      store(ptr + i, bytes[0](), local_block_val, non_local_block_val);
    }
  } else {
    Pointer idx(*this, "#idx");
    expr cond = idx.uge(ptr).both() && idx.ult(ptr + bytesize).both();
    store_lambda(idx, cond, bytes[0](), local_block_val, non_local_block_val);
  }
}

void Memory::memcpy(const expr &d, const expr &s, const expr &bytesize,
                    unsigned align_dst, unsigned align_src, bool is_move) {
  assert(!memory_unused());
  Pointer dst(*this, d), src(*this, s);
  dst.is_dereferenceable(bytesize, align_dst, true);
  src.is_dereferenceable(bytesize, align_src, false);
  if (!is_move)
    src.is_disjoint(bytesize, dst, bytesize);

  uint64_t n;
  if (bytesize.isUInt(n) && n <= 4) {
    auto old_local = local_block_val, old_nonlocal = non_local_block_val;
    for (unsigned i = 0; i < n; ++i) {
      store(dst + i, ::load(src + i, old_local, old_nonlocal),
            local_block_val, non_local_block_val);
    }
  } else {
    Pointer dst_idx(*this, "#idx");
    Pointer src_idx = src + (dst_idx.get_offset() - dst.get_offset());
    expr cond = dst_idx.uge(dst).both() && dst_idx.ult(dst + bytesize).both();
    store_lambda(dst_idx, cond,
                 ::load(src_idx, local_block_val, non_local_block_val),
                 local_block_val, non_local_block_val);

    // we don't know what was copied; it could have been a pointer
    did_pointer_store = true;
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

expr Memory::refined(const Memory &other) const {
  if (memory_unused())
    return true;

  Pointer p(*this, "#idx");
  Pointer q(other, p());
  return p.block_refined(q);
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
  ret.local_blk_kind.add(els.local_blk_kind);
  ret.did_pointer_store |= els.did_pointer_store;
  return ret;
}

bool Memory::operator<(const Memory &rhs) const {
  // FIXME: remove this once we move to C++20
  return
    tie(did_pointer_store, non_local_block_val, local_block_val,
        non_local_block_liveness, local_block_liveness, local_blk_addr,
        local_blk_size, local_blk_kind, local_avail_space) <
    tie(rhs.did_pointer_store, rhs.non_local_block_val, rhs.local_block_val,
        rhs.non_local_block_liveness, rhs.local_block_liveness,
        rhs.local_blk_addr, rhs.local_blk_size, rhs.local_blk_kind,
        rhs.local_avail_space);
}

}
