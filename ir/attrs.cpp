// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/attrs.h"
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
  return os;
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

static void
encodePtrAttrs(const State &s, const expr &ptrvalue,
               AndExpr &UB, expr &non_poison,
               uint64_t derefBytes, uint64_t derefOrNullBytes, uint64_t align,
               bool nonnull, bool nocapture) {
  Pointer p(s.getMemory(), ptrvalue);

  if (nonnull)
    non_poison &= !p.isNull();

  non_poison &= p.isNocapture().implies(nocapture);

  if (derefBytes || derefOrNullBytes) {
    // dereferenceable, byval (ParamAttrs), dereferenceable_or_null
    if (derefBytes)
      UB.add(p.isDereferenceable(derefBytes, align));
    if (derefOrNullBytes)
      UB.add(p.isDereferenceable(derefOrNullBytes, align)() || p.isNull());
  } else if (align != 1)
    // align
    non_poison &= p.isAligned(align);
}

pair<AndExpr, expr>
ParamAttrs::encode(const State &s, const StateValue &val, const Type &ty) const {
  AndExpr UB;
  expr new_non_poison = val.non_poison;

  if (ty.isPtrType())
    encodePtrAttrs(s, val.value, UB, new_non_poison, getDerefBytes(),
                   derefOrNullBytes, align, has(NonNull), has(NoCapture));

  if (poisonImpliesUB()) {
    UB.add(std::move(new_non_poison));
    new_non_poison = true;
  }

  return { std::move(UB), std::move(new_non_poison) };
}


bool FnAttrs::poisonImpliesUB() const {
  return has(Dereferenceable) || has(NoUndef) || has(NNaN) ||
         has(DereferenceableOrNull);
}

bool FnAttrs::undefImpliesUB() const {
  bool ub = has(NoUndef) || has(Dereferenceable) || has(DereferenceableOrNull);
  assert(!ub || poisonImpliesUB());
  return ub;
}

pair<AndExpr, expr>
FnAttrs::encode(const State &s, const StateValue &val, const Type &ty) const {
  AndExpr UB;
  expr new_non_poison = val.non_poison;

  if (has(FnAttrs::NNaN)) {
    assert(ty.isFloatType());
    new_non_poison &= !val.value.isNaN();
  }

  if (ty.isPtrType())
    encodePtrAttrs(s, val.value, UB, new_non_poison, derefBytes,
                   derefOrNullBytes, align, has(NonNull), false);

  if (poisonImpliesUB()) {
    UB.add(std::move(new_non_poison));
    new_non_poison = true;
  }

  return { std::move(UB), std::move(new_non_poison) };
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
