// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/attrs.h"
#include "ir/function.h"
#include "ir/globals.h"
#include "ir/instr.h"
#include "ir/memory.h"
#include "ir/state.h"
#include "ir/state_value.h"
#include "ir/type.h"
#include "util/compiler.h"
#include <cassert>

using namespace std;
using namespace smt;
using namespace util;

namespace IR {
ostream& operator<<(ostream &os, const ParamAttrs &attr) {
  if (attr.has(ParamAttrs::NonNull))
    os << "nonnull ";
  if (attr.has(ParamAttrs::ByVal))
    os << "byval(" << attr.blockSize << ") ";
  if (attr.has(ParamAttrs::NoCapture))
    os << "nocapture ";
  if (attr.has(ParamAttrs::NoRead))
    os << "noread ";
  if (attr.has(ParamAttrs::NoWrite))
    os << "nowrite ";
  if (attr.has(ParamAttrs::Dereferenceable))
    os << "dereferenceable(" << attr.derefBytes << ") ";
  if (attr.has(ParamAttrs::NoUndef))
    os << "noundef ";
  if (attr.has(ParamAttrs::NoFPClass))
    os << "nofpclass(" << attr.nofpclass << ") ";
  if (attr.has(ParamAttrs::Align))
    os << "align(" << attr.align << ") ";
  if (attr.has(ParamAttrs::Returned))
    os << "returned ";
  if (attr.has(ParamAttrs::NoAlias))
    os << "noalias ";
  if (attr.has(ParamAttrs::DereferenceableOrNull))
    os << "dereferenceable_or_null(" << attr.derefOrNullBytes << ") ";
  if (attr.has(ParamAttrs::ZeroExt))
    os << "zeroext ";
  if (attr.has(ParamAttrs::SignExt))
    os << "signext ";
  if (attr.has(ParamAttrs::InReg))
    os << "inreg ";
  if (attr.has(ParamAttrs::AllocPtr))
    os << "allocptr ";
  if (attr.has(ParamAttrs::AllocAlign))
    os << "allocalign ";
  if (attr.has(ParamAttrs::DeadOnUnwind))
    os << "dead_on_unwind ";
  if (attr.has(ParamAttrs::Writable))
    os << "writable ";
  return os;
}


static ostream& operator<<(ostream &os, FPDenormalAttrs::Type t) {
  const char *str = nullptr;
  switch (t) {
  case FPDenormalAttrs::IEEE:         str = "ieee"; break;
  case FPDenormalAttrs::PositiveZero: str = "positive-zero"; break;
  case FPDenormalAttrs::PreserveSign: str = "preserve-sign"; break;
  case FPDenormalAttrs::Dynamic:      str = "dynamic"; break;
  }
  return os << str;
}

void FPDenormalAttrs::print(ostream &os, bool is_fp32) const {
  if (input == IEEE && output == IEEE)
    return;
  os << " denormal-fp-math" << (is_fp32 ? "-f32=" : "=")
     << output << ',' << input;
}


ostream& operator<<(ostream &os, const FnAttrs &attr) {
  if (attr.has(FnAttrs::NNaN))
    os << " NNaN";
  if (attr.has(FnAttrs::NoReturn))
    os << " noreturn";
  if (attr.has(FnAttrs::Dereferenceable))
    os << " dereferenceable(" << attr.derefBytes << ')';
  if (attr.has(FnAttrs::NonNull))
    os << " nonnull";
  if (attr.has(FnAttrs::NoFree))
    os << " nofree";
  if (attr.has(FnAttrs::NoUndef))
    os << " noundef";
  if (attr.has(FnAttrs::NoFPClass))
    os << " nofpclass(" << attr.nofpclass << ')';
  if (attr.has(FnAttrs::Align))
    os << " align(" << attr.align << ')';
  if (attr.has(FnAttrs::NoThrow))
    os << " nothrow";
  if (attr.has(FnAttrs::NoAlias))
    os << " noalias";
  if (attr.has(FnAttrs::WillReturn))
    os << " willreturn";
  if (attr.has(FnAttrs::DereferenceableOrNull))
    os << " dereferenceable_or_null(" << attr.derefOrNullBytes << ')';
  if (attr.has(FnAttrs::NullPointerIsValid))
    os << " null_pointer_is_valid";
  if (attr.has(FnAttrs::ZeroExt))
    os << " zeroext";
  if (attr.has(FnAttrs::SignExt))
    os << " signext";
  if (!attr.allocfamily.empty())
    os << " alloc-family(" << attr.allocfamily << ')';
  if (attr.allockind != 0) {
    os << " allockind(";
    bool first = true;
    auto print = [&](AllocKind kind, const char *str) {
      if (attr.has(kind)) {
        if (!first) os << ", ";
        os << str;
        first = false;
      }
    };
    print(AllocKind::Alloc, "alloc");
    print(AllocKind::Realloc, "realloc");
    print(AllocKind::Free, "free");
    print(AllocKind::Uninitialized, "uninitialized");
    print(AllocKind::Zeroed, "zeroed");
    print(AllocKind::Aligned, "aligned");
    os << ')';
  }
  if (attr.has(FnAttrs::AllocSize)) {
    os << " allocsize(" << attr.allocsize_0;
    if (attr.allocsize_1 != -1u)
      os << ", " << attr.allocsize_1;
    os << ')';
  }

  attr.fp_denormal.print(os);
  if (attr.fp_denormal32)
    attr.fp_denormal32->print(os, true);
  if (attr.has(FnAttrs::Asm))
    os << " asm";
  return os << attr.mem;
}


// format ..rw..
bool MemoryAccess::canRead(AccessType ty) const {
  return (val >> (2 * ty)) & 2;
}

bool MemoryAccess::canWrite(AccessType ty) const {
  return (val >> (2 * ty)) & 1;
}

bool MemoryAccess::canOnlyRead(AccessType ty) const {
  for (unsigned i = 0; i < NumTypes; ++i) {
    if (i != ty && canRead(AccessType(i)))
      return false;
  }
  return canRead(ty);
}

bool MemoryAccess::canOnlyWrite(AccessType ty) const {
  for (unsigned i = 0; i < NumTypes; ++i) {
    if (i != ty && canWrite(AccessType(i)))
      return false;
  }
  return canWrite(ty);
}

bool MemoryAccess::canAccessAnything() const {
  return canReadAnything() && canWriteAnything();
}

bool MemoryAccess::canReadAnything() const {
  for (unsigned i = 0; i < NumTypes; ++i) {
    if (!canRead(AccessType(i)))
      return false;
  }
  return true;
}

bool MemoryAccess::canWriteAnything() const {
  for (unsigned i = 0; i < NumTypes; ++i) {
    if (!canWrite(AccessType(i)))
      return false;
  }
  return true;
}

bool MemoryAccess::canReadSomething() const {
  for (unsigned i = 0; i < NumTypes; ++i) {
    if (canRead(AccessType(i)))
      return true;
  }
  return false;
}

bool MemoryAccess::canWriteSomething() const {
  for (unsigned i = 0; i < NumTypes; ++i) {
    if (canWrite(AccessType(i)))
      return true;
  }
  return false;
}

void MemoryAccess::setFullAccess() {
  for (unsigned i = 0; i < NumTypes; ++i) {
    setCanAlsoAccess(AccessType(i));
  }
}

void MemoryAccess::setCanOnlyRead() {
  setNoAccess();
  for (unsigned i = 0; i < NumTypes; ++i) {
    setCanAlsoRead(AccessType(i));
  }
}

void MemoryAccess::setCanOnlyWrite() {
  setNoAccess();
  for (unsigned i = 0; i < NumTypes; ++i) {
    setCanOnlyWrite(AccessType(i));
  }
}

void MemoryAccess::setCanOnlyRead(AccessType ty) {
  setNoAccess();
  setCanAlsoRead(ty);
}

void MemoryAccess::setCanOnlyWrite(AccessType ty) {
  setNoAccess();
  setCanAlsoWrite(ty);
}

void MemoryAccess::setCanOnlyAccess(AccessType ty) {
  setCanOnlyRead(ty);
  setCanAlsoWrite(ty);
}

void MemoryAccess::setCanAlsoRead(AccessType ty) {
  val |= 2u << (ty * 2);
}

void MemoryAccess::setCanAlsoWrite(AccessType ty) {
  val |= 1u << (ty * 2);
}

void MemoryAccess::setCanAlsoAccess(AccessType ty) {
  setCanAlsoRead(ty);
  setCanAlsoWrite(ty);
}

ostream& operator<<(ostream &os, const MemoryAccess &a) {
  if (a.val == 0)
    return os << " memory(none)";

  if (a.canReadAnything())
    return a.canWriteAnything() ? os : (os << " memory(read)");

  if (a.canWriteAnything())
    return os << " memory(write)";

  array<const char*, 5> vals = {
    "argmem",
    "globals",
    "inaccessiblemem",
    "errno",
    "other",
  };
  static_assert(vals.size() == MemoryAccess::NumTypes);

  os << " memory(";

  unsigned i = 0;
  bool first = true;
  for (auto *str : vals) {
    auto ty = MemoryAccess::AccessType(i++);
    if (a.canRead(ty)) {
      if (!first) os << ", ";
      os << str << (a.canWrite(ty) ? ": readwrite" : ": read");
      first = false;
    } else if (a.canWrite(ty)) {
      if (!first) os << ", ";
      os << str << ": write";
      first = false;
    }
  }
  assert(i == MemoryAccess::NumTypes);

  return os << ')';
}


bool ParamAttrs::refinedBy(const ParamAttrs &other) const {
  // check attributes that may give UB to the caller if added
  unsigned attrs =
    ByVal |
    NoUndef |
    Writable
  ;

  auto other_params = (other.bits & attrs);
  if ((bits & other_params) != other_params)
    return false;

  // check attributes that cannot change
  attrs =
    SignExt |
    ZeroExt |
    InReg
  ;
  if ((bits & attrs) != (other.bits & attrs))
    return false;

  return blockSize == other.blockSize;
}

bool ParamAttrs::poisonImpliesUB() const {
  return has(ByVal) ||
         has(Dereferenceable) ||
         has(DereferenceableOrNull) ||
         has(NoUndef) ||
         has(Writable);
}

uint64_t ParamAttrs::getDerefBytes() const {
  uint64_t bytes = 0;
  if (has(ParamAttrs::Dereferenceable))
    bytes = derefBytes;
  // byval copies bytes; the ptr needs to be dereferenceable
  if (has(ParamAttrs::ByVal))
    bytes = max(bytes, blockSize);
  return bytes;
}

uint64_t ParamAttrs::maxAccessSize() const {
  uint64_t bytes = getDerefBytes();
  if (has(ParamAttrs::DereferenceableOrNull))
    bytes = max(bytes, derefOrNullBytes);
  return round_up(bytes, align);
}

void ParamAttrs::merge(const ParamAttrs &other) {
  bits            |= other.bits;
  derefBytes       = max(derefBytes, other.derefBytes);
  derefOrNullBytes = max(derefOrNullBytes, other.derefOrNullBytes);
  blockSize        = max(blockSize, other.blockSize);
  align            = max(align, other.align);
}

static expr
encodePtrAttrs(State &s, const expr &ptrvalue, uint64_t derefBytes,
               uint64_t derefOrNullBytes, uint64_t align, bool nonnull,
               bool nocapture, bool writable, const expr &allocsize,
               Value *allocalign, bool isdecl) {
  auto &m = s.getMemory();
  Pointer p(m, ptrvalue);
  expr non_poison(true);

  if (nonnull)
    non_poison &= !p.isNull();

  non_poison &= p.isNocapture().implies(nocapture);

  if (derefBytes || derefOrNullBytes || allocsize.isValid()) {
    // dereferenceable, byval (ParamAttrs), dereferenceable_or_null
    if (derefBytes)
      s.addUB(merge(p.isDereferenceable(derefBytes, align, writable, true)));
    if (derefOrNullBytes)
      s.addUB(p.isNull() ||
              merge(p.isDereferenceable(derefOrNullBytes, align, writable,
                                        true)));
    if (allocsize.isValid())
      s.addUB(p.isNull() ||
              merge(p.isDereferenceable(allocsize, align, writable, true)));
  } else if (align != 1) {
    non_poison &= p.isAligned(align);
    if (isdecl)
      s.addUB(merge(p.isDereferenceable(1, 1, false, true))
                .implies(merge(p.isDereferenceable(1, align, false, true))));
  }

  if (allocalign) {
    Pointer p(m, ptrvalue);
    auto &align = s[*allocalign];
    auto bw = max(align.bits(), allocsize.bits());
    non_poison &= align.non_poison;
    // pointer must be null if alignment is not a power of 2
    // or size is not a multiple of alignment
    non_poison &= p.isNull() ||
      (p.isAligned(align.value) &&
       allocsize.zextOrTrunc(bw).urem(align.value.zextOrTrunc(bw)) == 0);
  }
  return non_poison;
}

StateValue ParamAttrs::encode(State &s, StateValue &&val, const Type &ty,
                              bool isdecl) const{
  if (has(NoFPClass)) {
    assert(ty.isFloatType());
    val.non_poison &= !isfpclass(val.value, ty, nofpclass);
  }

  if (ty.isPtrType())
    val.non_poison &=
      encodePtrAttrs(s, val.value, getDerefBytes(), derefOrNullBytes, align,
                     has(NonNull), has(NoCapture), has(Writable), {}, nullptr,
                     isdecl);

  if (poisonImpliesUB()) {
    s.addUB(std::move(val.non_poison));
    val.non_poison = true;
  }

  return std::move(val);
}


void FnAttrs::inferImpliedAttributes() {
  if (!mem.canWriteSomething())
    set(NoFree);
}

pair<expr,expr>
FnAttrs::computeAllocSize(State &s,
                          const vector<pair<Value*, ParamAttrs>> &args) const {
  if (!has(AllocSize))
    return { {}, true };

  auto &arg0 = s[*args[allocsize_0].first];
  s.addUB(arg0.non_poison);
  expr allocsize = arg0.value.zextOrTrunc(bits_size_t);
  expr np_size   = arg0.non_poison;

  auto check_trunc = [&](const expr &var) {
    if (var.bits() > bits_size_t)
      np_size &= var.extract(var.bits()-1, bits_size_t) == 0;
  };
  check_trunc(arg0.value);

  if (allocsize_1 != -1u) {
    auto &arg1 = s[*args[allocsize_1].first];
    s.addUB(arg1.non_poison);

    auto v = arg1.value.zextOrTrunc(bits_size_t);
    np_size  &= arg1.non_poison;
    np_size  &= allocsize.mul_no_uoverflow(v);
    allocsize = allocsize * v;
    check_trunc(arg1.value);
  }
  return { std::move(allocsize), std::move(np_size) };
}

bool FnAttrs::isNonNull() const {
  return has(NonNull) ||
         (!has(NullPointerIsValid) && derefBytes > 0);
}

bool FnAttrs::poisonImpliesUB() const {
  return has(NoUndef) || has(Dereferenceable) || has(DereferenceableOrNull) ||
         has(AllocSize) || has(NNaN);
}

void FnAttrs::setFPDenormal(FPDenormalAttrs attr, unsigned bits) {
  switch (bits) {
  case 0:  fp_denormal = attr; break;
  case 32: fp_denormal32 = attr; break;
  default: UNREACHABLE();
  }
}

FPDenormalAttrs FnAttrs::getFPDenormal(const Type &ty) const {
  switch (ty.bits()) {
  case 32: return fp_denormal32.value_or(fp_denormal);
  default: return fp_denormal;
  }
}

bool FnAttrs::refinedBy(const FnAttrs &other) const {
  // check attributes that can't be added, removed, or changed
  unsigned attrs =
    NullPointerIsValid
  ;

  if ((bits & attrs) != (other.bits & attrs))
    return false;

  if (has(NoReturn) && other.has(WillReturn))
    return false;

  return fp_denormal == other.fp_denormal &&
         fp_denormal32 == other.fp_denormal32;
}

StateValue FnAttrs::encode(State &s, StateValue &&val, const Type &ty,
                           const expr &allocsize,
                           Value *allocalign) const {
  if (has(NNaN)) {
    assert(ty.isFloatType());
    val.non_poison &= !ty.getAsFloatType()->getFloat(val.value).isNaN();
  }

  if (has(NoFPClass)) {
    assert(ty.isFloatType());
    val.non_poison &= !isfpclass(val.value, ty, nofpclass);
  }

  if (ty.isPtrType())
    val.non_poison &=
      encodePtrAttrs(s, val.value, derefBytes, derefOrNullBytes, align,
                     has(NonNull), false, false, allocsize, allocalign, false);

  if (poisonImpliesUB()) {
    s.addUB(std::move(val.non_poison));
    val.non_poison = true;
  }

  return std::move(val);
}


expr isfpclass(const expr &v, const Type &ty, uint16_t mask) {
  if (mask == 1023)
    return true;

  auto *fpty = ty.getAsFloatType();
  auto a = fpty->getFloat(v);
  OrExpr result;
  if (mask & (1 << 0))
    result.add(fpty->isNaN(v, true));
  if (mask & (1 << 1))
    result.add(fpty->isNaN(v, false));

  auto check = [&](unsigned idx_neg, unsigned idx_pos, auto test) {
    unsigned mask_neg  = 1u << idx_neg;
    unsigned mask_pos  = 1u << idx_pos;
    unsigned mask_both = mask_neg | mask_pos;

    if ((mask & mask_both) == mask_both) {
      result.add(test());
    } else if (mask & mask_neg) {
      result.add(a.isFPNegative() && test());
    } else if (mask & mask_pos) {
      result.add(!a.isFPNegative() && test());
    }
  };
#define CHECK(neg, pos, fn) check(neg, pos, [&a]() { return a.fn(); })

  CHECK(5, 6, isFPZero);
  CHECK(4, 7, isFPSubNormal);
  CHECK(3, 8, isFPNormal);
  CHECK(2, 9, isInf);

#undef CHECK

  return std::move(result)();
}


ostream& operator<<(ostream &os, const FastMathFlags &fm) {
  if (fm.flags == FastMathFlags::FastMath)
    return os << "fast ";

  if (fm.flags & FastMathFlags::NNaN)
    os << "nnan ";
  if (fm.flags & FastMathFlags::NInf)
    os << "ninf ";
  if (fm.flags & FastMathFlags::NSZ)
    os << "nsz ";
  if (fm.flags & FastMathFlags::ARCP)
    os << "arcp ";
  if (fm.flags & FastMathFlags::Contract)
    os << "contract ";
  if (fm.flags & FastMathFlags::Reassoc)
    os << "reassoc ";
  if (fm.flags & FastMathFlags::AFN)
    os << "afn ";
  return os;
}


smt::expr FpRoundingMode::toSMT() const {
  switch (mode) {
  case FpRoundingMode::Dynamic: UNREACHABLE();
  case FpRoundingMode::Default:
  case FpRoundingMode::RNE:     return expr::rne();
  case FpRoundingMode::RNA:     return expr::rna();
  case FpRoundingMode::RTP:     return expr::rtp();
  case FpRoundingMode::RTN:     return expr::rtn();
  case FpRoundingMode::RTZ:     return expr::rtz();
  }
  UNREACHABLE();
}

ostream& operator<<(std::ostream &os, FpRoundingMode rounding) {
  const char *str = nullptr;
  switch (rounding.mode) {
  case FpRoundingMode::Dynamic: str = "dynamic"; break;
  case FpRoundingMode::RNE:     str = "tonearest"; break;
  case FpRoundingMode::RNA:     str = "tonearestaway"; break;
  case FpRoundingMode::RTP:     str = "upward"; break;
  case FpRoundingMode::RTN:     str = "downward"; break;
  case FpRoundingMode::RTZ:     str = "towardzero"; break;
  case FpRoundingMode::Default: UNREACHABLE();
  }
  return os << str;
}

ostream& operator<<(std::ostream &os, FpExceptionMode ex) {
  const char *str = nullptr;
  switch (ex.mode) {
  case FpExceptionMode::Ignore:  str = "ignore"; break;
  case FpExceptionMode::MayTrap: str = "maytrap"; break;
  case FpExceptionMode::Strict:  str = "strict"; break;
  }
  return os << str;
}

ostream& operator<<(std::ostream &os, const TailCallInfo &tci) {
  const char *str = nullptr;
  switch (tci.type) {
    case TailCallInfo::None:     str = ""; break;
    case TailCallInfo::Tail:     str = "tail "; break;
    case TailCallInfo::MustTail: str = "musttail "; break;
  }
  return os << str;
}

void TailCallInfo::checkTailCall(const Instr &i, State &s) const {
  bool preconditions_OK = true;
  assert(type != TailCallInfo::None);

  auto *callee = dynamic_cast<const FnCall *>(&i);
  if (callee) {
    for (const auto &[arg, attrs] : callee->getArgs()) {
      bool callee_has_byval = attrs.has(ParamAttrs::ByVal);
      if (dynamic_cast<const Alloc *>(arg) && !callee_has_byval) {
        preconditions_OK = false;
        break;
      }
      if (auto *input = dynamic_cast<const Input *>(arg)) {
        bool caller_has_byval = input->hasAttribute(ParamAttrs::ByVal);
        if (callee_has_byval != caller_has_byval) {
          preconditions_OK = false;
          break;
        }
      }
    }
  } else {
    // Handling memcpy / memcmp et alia.
    for (const auto &op : i.operands()) {
      if (dynamic_cast<const Alloc *>(op)) {
        preconditions_OK = false;
        break;
      }
    }
  }

  if (callee && type == TailCallInfo::MustTail) {
    bool callee_is_vararg = callee->getVarArgIdx() != -1u;
    bool caller_is_vararg = s.getFn().isVarArgs();
    if (!has_same_calling_convention || (callee_is_vararg && !caller_is_vararg))
      preconditions_OK = false;
  }

  if (preconditions_OK && type == TailCallInfo::MustTail) {
    bool found = false;
    const auto &instrs = s.getFn().bbOf(i).instrs();
    auto it = instrs.begin();
    for (auto e = instrs.end(); it != e; ++it) {
      if (&*it == &i) {
        found = true;
        break;
      }
    }
    assert(found);

    ++it;
    auto &next_instr = *it;
    if (auto *ret = dynamic_cast<const Return *>(&next_instr)) {
      if (ret->getType().isVoid() && i.getType().isVoid())
        return;
      auto *ret_val = ret->operands()[0];
      if (ret_val == &i)
        return;
    }

    preconditions_OK = false;
  }

  if (!preconditions_OK) {
    // Preconditions unsatifisfied or refinement for musttail failed, hence UB.
    s.addUB(expr(false));
  }
}

}
