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
  return os;
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
               bool nonnull) {
  Pointer p(s.getMemory(), ptrvalue);

  if (nonnull)
    non_poison &= p.isNonZero();

  if (derefBytes || derefOrNullBytes) {
    // dereferenceable, byval (ParamAttrs), dereferenceable_or_null
    if (derefBytes)
      UB.add(p.isDereferenceable(derefBytes, align));
    if (derefOrNullBytes)
      UB.add(p.isDereferenceable(derefOrNullBytes, align)() || !p.isNonZero());
  } else if (align != 1)
    // align
    non_poison &= p.isAligned(align);
}

pair<AndExpr, expr>
ParamAttrs::encode(const State &s, const StateValue &val, const Type &ty) const {
  AndExpr UB;
  expr new_non_poison = val.non_poison;

  if (ty.isPtrType())
    encodePtrAttrs(s, val.value, UB, new_non_poison,
                   getDerefBytes(), derefOrNullBytes, align, has(NonNull));

  if (poisonImpliesUB()) {
    UB.add(move(new_non_poison));
    new_non_poison = true;
  }

  return { move(UB), move(new_non_poison) };
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
    UB.add(val.non_poison.implies(!val.value.isNaN()));
  }

  if (ty.isPtrType())
    encodePtrAttrs(s, val.value, UB, new_non_poison,
                   derefBytes, derefOrNullBytes, align, has(NonNull));

  if (poisonImpliesUB()) {
    UB.add(move(new_non_poison));
    new_non_poison = true;
  }

  return { move(UB), move(new_non_poison) };
}

}
