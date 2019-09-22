// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/state_value.h"

using namespace smt;
using namespace std;

namespace IR {

StateValue StateValue::mkIf(const expr &cond, const StateValue &then,
                            const StateValue &els) {
  return { expr::mkIf(cond, then.value, els.value),
           expr::mkIf(cond, then.non_poison, els.non_poison) };
}

StateValue StateValue::zext(unsigned amount) const {
  return { value.zext(amount),
           non_poison.isBool() ? expr(non_poison) : non_poison.zext(amount) };
}

StateValue StateValue::zextOrTrunc(unsigned tobw) const {
  return
    { value.zextOrTrunc(tobw),
      non_poison.isBool() ? expr(non_poison) : non_poison.zextOrTrunc(tobw) };
}

StateValue StateValue::concat(const StateValue &other) const {
  return { value.concat(other.value), non_poison.concat(other.non_poison) };
}

bool StateValue::eq(const StateValue &other) const {
  return value.eq(other.value) && non_poison.eq(other.non_poison);
}

StateValue StateValue::subst(const vector<pair<expr, expr>> &repls) const {
  return { value.subst(repls), non_poison.subst(repls) };
}

ostream& operator<<(ostream &os, const StateValue &val) {
  return os << val.value << " / " << val.non_poison;
}

}
