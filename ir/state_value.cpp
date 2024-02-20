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
  return { value.trunc(bw_val),
           non_poison.isBool() ? expr(non_poison) : non_poison.trunc(bw_np) };
}

StateValue StateValue::zextOrTrunc(unsigned tobw) const {
  return
    { value.zextOrTrunc(tobw),
      (bits_poison_per_byte == 1 || non_poison.isBool())
        ? expr(non_poison) : non_poison.zextOrTrunc(tobw) };
}

StateValue StateValue::concat(const StateValue &other) const {
  return { value.concat(other.value),
           non_poison.isBool() ? non_poison && other.non_poison
                               : non_poison.concat(other.non_poison) };
}

void StateValue::setNotPoison() {
  if (non_poison.isValid()) {
    if (non_poison.isBool())
      non_poison = true;
    else {
      assert(non_poison.isBV());
      non_poison = expr::mkInt(-1, non_poison);
    }
  }
}

bool StateValue::isValid() const {
  return value.isValid() && non_poison.isValid();
}

expr StateValue::operator==(const StateValue &other) const {
  return non_poison == other.non_poison && value == other.value;
}

expr StateValue::implies(const StateValue &other) const {
  return non_poison.implies(other.non_poison && value == other.value);
}

bool StateValue::eq(const StateValue &other) const {
  return value.eq(other.value) && non_poison.eq(other.non_poison);
}

set<expr> StateValue::vars() const {
  return expr::vars({ &value, &non_poison });
}

StateValue StateValue::subst(const expr &from, const expr &to) const {
  return { value.subst(from, to), non_poison.subst(from, to) };
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
    np = a.eq(v2) ? std::move(b) : std::move(a);
  } else {
    assert(non_poison.isBool() && non_poison.isConst());
    np = non_poison;
  }
  return { then.eq(v1) ? std::move(els) : std::move(then), std::move(np) };
}

StateValue StateValue::simplify() const {
  return { value.simplify(), non_poison.simplify() };
}

ostream& operator<<(ostream &os, const StateValue &val) {
  return os << val.value << " / " << val.non_poison;
}

}
