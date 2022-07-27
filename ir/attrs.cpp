// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/attrs.h"
#include "ir/globals.h"
#include "ir/memory.h"
#include "ir/state.h"
#include "ir/state_value.h"
#include "ir/type.h"
#include <cassert>

using namespace std;
using namespace smt;

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
  if (attr.has(ParamAttrs::Align))
    os << "align(" << attr.align << ") ";
  if (attr.has(ParamAttrs::Returned))
    os << "returned ";
  if (attr.has(ParamAttrs::NoAlias))
    os << "noalias ";
  if (attr.has(ParamAttrs::DereferenceableOrNull))
    os << "dereferenceable_or_null(" << attr.derefOrNullBytes << ") ";
  if (attr.has(ParamAttrs::AllocPtr))
    os << "allocptr ";
  if (attr.has(ParamAttrs::AllocAlign))
    os << "allocalign ";
  return os;
}


static ostream& operator<<(ostream &os, FPDenormalAttrs::Type t) {
  const char *str = nullptr;
  switch (t) {
  case FPDenormalAttrs::IEEE:         str = "ieee"; break;
  case FPDenormalAttrs::PreserveSign: str = "preserve-sign"; break;
  case FPDenormalAttrs::PositiveZero: str = "positive-zero"; break;
  }
  return os << str;
}

void FPDenormalAttrs::print(ostream &os, bool is_fp32) const {
  os << " denormal-fp-math" << (is_fp32 ? "-f32=" : "=")
     << output << ',' << input;
}


ostream& operator<<(ostream &os, const FnAttrs &attr) {
  if (attr.has(FnAttrs::NoRead))
    os << " noread";
  if (attr.has(FnAttrs::NoWrite))
    os << " nowrite";
  if (attr.has(FnAttrs::ArgMemOnly))
    os << " argmemonly";
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
  if (attr.has(FnAttrs::InaccessibleMemOnly))
    os << " inaccessiblememonly";
  if (attr.has(FnAttrs::NullPointerIsValid))
    os << " null_pointer_is_valid";
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
  return os;
}

bool ParamAttrs::refinedBy(const ParamAttrs &other) const {
  // check attributes that are properties of the caller
  unsigned attrs =
    NonNull |
    Dereferenceable |
    NoUndef |
    Align |
    NoAlias |
    DereferenceableOrNull
  ;

  auto other_params = (other.bits & attrs);
  if ((bits & other_params) != other_params)
    return false;

  return derefBytes == other.derefBytes &&
         derefOrNullBytes == other.derefOrNullBytes &&
         blockSize == other.blockSize &&
         align == other.align;
}

bool ParamAttrs::poisonImpliesUB() const {
  return has(Dereferenceable) || has(NoUndef) || has(ByVal) ||
         has(DereferenceableOrNull);
}

bool ParamAttrs::undefImpliesUB() const {
  bool ub = has(NoUndef) || has(Dereferenceable) || has(ByVal) ||
            has(DereferenceableOrNull);
  assert(!ub || poisonImpliesUB());
  return ub;
}

uint64_t ParamAttrs::getDerefBytes() const {
  uint64_t bytes = 0;
  if (has(ParamAttrs::Dereferenceable))
    bytes = derefBytes;
  // byval copies bytes; the ptr needs to be dereferenceable
  if (has(ParamAttrs::ByVal))
    bytes = max(bytes, (uint64_t)blockSize);
  return bytes;
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
               bool nocapture, const expr &deref_expr) {
  auto &m = s.getMemory();
  Pointer p(m, ptrvalue);
  expr non_poison(true);

  if (nonnull)
    non_poison &= !p.isNull();

  non_poison &= p.isNocapture().implies(nocapture);

  if (derefBytes || derefOrNullBytes || deref_expr.isValid()) {
    // dereferenceable, byval (ParamAttrs), dereferenceable_or_null
    if (derefBytes)
      s.addUB(Pointer(m, ptrvalue).isDereferenceable(derefBytes, align));
    if (derefOrNullBytes)
      s.addUB(
        Pointer(m, ptrvalue).isDereferenceable(derefOrNullBytes, align)() ||
        p.isNull());
    if (deref_expr.isValid())
      s.addUB(
        Pointer(m, ptrvalue).isDereferenceable(deref_expr, align, false)() ||
        p.isNull());
  } else if (align > 1)
    non_poison &= Pointer(m, ptrvalue).isAligned(align);

  // TODO: handle alloc align

  return non_poison;
}

StateValue ParamAttrs::encode(State &s, StateValue &&val, const Type &ty) const{
  if (ty.isPtrType())
    val.non_poison &=
      encodePtrAttrs(s, val.value, getDerefBytes(), derefOrNullBytes, align,
                     has(NonNull), has(NoCapture), {});

  if (poisonImpliesUB()) {
    s.addUB(std::move(val.non_poison));
    val.non_poison = true;
  }

  return std::move(val);
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
  return has(Dereferenceable) || has(NoUndef) || has(NNaN) ||
         has(DereferenceableOrNull) || has(AllocSize);
}

bool FnAttrs::undefImpliesUB() const {
  bool ub = has(NoUndef) || has(Dereferenceable) || has(DereferenceableOrNull)||
            has(AllocSize);
  assert(!ub || poisonImpliesUB());
  return ub;
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

  return fp_denormal == other.fp_denormal &&
         fp_denormal32 == other.fp_denormal32;
}

StateValue FnAttrs::encode(State &s, StateValue &&val, const Type &ty,
                           const expr &allocsize) const {
  if (has(FnAttrs::NNaN)) {
    assert(ty.isFloatType());
    val.non_poison &= !val.value.isNaN();
  }

  if (ty.isPtrType())
    val.non_poison &=
      encodePtrAttrs(s, val.value, derefBytes, derefOrNullBytes, align,
                     has(NonNull), false, allocsize);

  if (poisonImpliesUB()) {
    s.addUB(std::move(val.non_poison));
    val.non_poison = true;
  }

  return std::move(val);
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
  case FpRoundingMode::RNE:     return expr::rne();
  case FpRoundingMode::RNA:     return expr::rna();
  case FpRoundingMode::RTP:     return expr::rtp();
  case FpRoundingMode::RTN:     return expr::rtn();
  case FpRoundingMode::RTZ:     return expr::rtz();
  case FpRoundingMode::Default: UNREACHABLE();
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

}
