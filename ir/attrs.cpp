// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/attrs.h"
#include "util/compiler.h"
#include <cassert>

using namespace std;
using namespace util;

namespace IR {
ostream& operator<<(ostream &os, const ParamAttrs &attr) {
  if (attr.has(ParamAttrs::NonNull))
    os << "nonnull ";
  if (attr.has(ParamAttrs::ByVal))
    os << "byval(" << attr.blockSize << ") ";
  if (attr.has(ParamAttrs::NoCapture))
    os << "nocapture ";
  if (attr.has(ParamAttrs::ReadOnly))
    os << "readonly ";
  if (attr.has(ParamAttrs::ReadNone))
    os << "readnone ";
  if (attr.has(ParamAttrs::Dereferenceable))
    os << "dereferenceable(" << attr.derefBytes << ") ";
  if (attr.has(ParamAttrs::NoUndef))
    os << "noundef ";
  if (attr.has(ParamAttrs::Align))
    os << "align(" << (1ull << attr.align) << ") ";
  if (attr.has(ParamAttrs::Returned))
    os << "returned ";
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
    os << " dereferenceable(" << attr.derefBytes << ")";
  if (attr.has(FnAttrs::NonNull))
    os << " nonnull";
  if (attr.has(FnAttrs::NoFree))
    os << " nofree";
  if (attr.has(FnAttrs::NoUndef))
    os << " noundef";
  if (attr.has(FnAttrs::Align))
    os << " align(" << (1ull << attr.align) << ")";
  return os;
}

uint64_t ParamAttrs::getAlign() const {
  assert(has(Align));
  return 1ull << align;
}

void ParamAttrs::setAlign(uint64_t a) {
  assert(has(Align));
  bool power2 = is_power2(a, &align);
  assert(power2);
}

bool ParamAttrs::undefImpliesUB() const {
  bool ub = has(NoUndef);
  assert(!ub || poisonImpliesUB());
  return ub;
}

bool ParamAttrs::operator==(const ParamAttrs &rhs) const {
  if (bits != rhs.bits)
    return false;

  if (has(Dereferenceable) && derefBytes != rhs.derefBytes)
    return false;
  if (has(ByVal) && blockSize != rhs.blockSize)
    return false;
  if (has(Align) && align != rhs.align)
    return false;

  return true;
}

uint64_t FnAttrs::getAlign() const {
  assert(has(Align));
  return 1ull << align;
}

void FnAttrs::setAlign(uint64_t a) {
  assert(has(Align));
  bool power2 = is_power2(a, &align);
  assert(power2);
}

bool FnAttrs::undefImpliesUB() const {
  bool ub = has(NoUndef);
  assert(!ub || poisonImpliesUB());
  return ub;
}

bool FnAttrs::operator==(const FnAttrs &rhs) const {
  if (bits != rhs.bits)
    return false;

  if (has(Dereferenceable) && derefBytes != rhs.derefBytes)
    return false;
  if (has(Align) && align != rhs.align)
    return false;

  return true;
}
}
