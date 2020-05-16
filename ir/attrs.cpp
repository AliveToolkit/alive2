// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/attrs.h"

using namespace std;

namespace IR {
ostream& operator<<(ostream &os, const ParamAttrs &attr) {
  if (attr.has(ParamAttrs::NonNull))
    os << "nonnull ";
  if (attr.has(ParamAttrs::ByVal))
    os << "byval ";
  if (attr.has(ParamAttrs::NoCapture))
    os << "nocapture ";
  if (attr.has(ParamAttrs::ReadOnly))
    os << "readonly ";
  if (attr.has(ParamAttrs::ReadNone))
    os << "readnone ";
  if (attr.has(ParamAttrs::Dereferenceable))
    os << "dereferenceable(" << attr.derefBytes << ") ";
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
  return os;
}

}
