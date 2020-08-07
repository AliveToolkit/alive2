// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/state_value.h"
#include "ir/globals.h"
#include "util/compiler.h"
#include <tuple>

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
           (bits_poison_per_byte == 1 || non_poison.isBool())
             ? expr(non_poison) : non_poison.zext(amount) };
}

StateValue StateValue::trunc(unsigned bw_val, unsigned bw_np) const {
  return { value.trunc(bw_val), non_poison.trunc(bw_np) };
}

StateValue StateValue::zextOrTrunc(unsigned tobw) const {
  return
    { value.zextOrTrunc(tobw),
      (bits_poison_per_byte == 1 || non_poison.isBool())
        ? expr(non_poison) : non_poison.zextOrTrunc(tobw) };
}

StateValue StateValue::concat(const StateValue &other) const {
  return { value.concat(other.value),
           non_poison.isBool() ? expr(non_poison)
                               : non_poison.concat(other.non_poison) };
}

bool StateValue::isValid() const {
  return value.isValid() && non_poison.isValid();
}

bool StateValue::eq(const StateValue &other) const {
  return value.eq(other.value) && non_poison.eq(other.non_poison);
}

set<expr> StateValue::vars() const {
  return expr::vars({ &value, &non_poison });
}

StateValue StateValue::subst(const vector<pair<expr, expr>> &repls) const {
  if (!value.isValid() || !non_poison.isValid())
    return { value.subst(repls), non_poison.subst(repls) };

  // create dummy expr so we can do a single subst (more efficient than 2)
  expr v1 = expr::mkVar("#1", value);
  expr v2 = expr::mkVar("#2", non_poison);
  expr val = expr::mkIf(non_poison.cmp_eq(v2, false), value, v1).subst(repls);

  expr cond, then, els, a, b, np;
  ENSURE(val.isIf(cond, then, els));
  if (cond.isEq(a, b)) {
    np = a.eq(v2) ? move(b) : move(a);
  } else {
    assert(non_poison.isBool() && non_poison.isConst());
    np = non_poison;
  }
  return { then.eq(v1) ? move(els) : move(then), move(np) };
}

bool StateValue::operator<(const StateValue &rhs) const {
  return tie(value, non_poison) < tie(rhs.value, rhs.non_poison);
}

ostream& operator<<(ostream &os, const StateValue &val) {
  return os << val.value << " / " << val.non_poison;
}

}
