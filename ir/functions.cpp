// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/functions.h"

using namespace smt;

namespace IR {

expr PtrInput::implies(const PtrInput &rhs) const {
  return implies_attrs(rhs) && val == rhs.val && idx == rhs.idx;
}

expr PtrInput::implies_attrs(const PtrInput &rhs) const {
  return byval == rhs.byval &&
         rhs.noread   .implies(noread) &&
         rhs.nowrite  .implies(nowrite) &&
         rhs.nocapture.implies(nocapture);
}

}
