// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/precondition.h"
#include "smt/expr.h"
#include "smt/solver.h"
#include "util/compiler.h"
#include <cassert>
#include <sstream>

using namespace smt;
using namespace std;
using namespace util;

namespace IR {

expr Predicate::getTypeConstraints(const Function &f) const {
  return true;
}

void Predicate::fixupTypes(const Model &m) {
  // do nothing
}


void BoolPred::print(ostream &os) const {
  os << '(';
  lhs.print(os);
  os << ") " << (pred == AND ? "&&" : "||") << " (";
  rhs.print(os);
  os << ')';
}

expr BoolPred::toSMT(State &s) const {
  auto a = lhs.toSMT(s);
  auto b = rhs.toSMT(s);
  switch (pred) {
  case AND: return a && b;
  case OR:  return a || b;
  }
  UNREACHABLE();
}


// name, num_args
static pair<const char*,unsigned> fn_data[] = {
  {"WillNotOverflowSignedAdd", 2},
  {"WillNotOverflowUnsignedAdd", 2},
  {"WillNotOverflowSignedSub", 2},
  {"WillNotOverflowUnsignedSub", 2},
  {"WillNotOverflowSignedMul", 2},
  {"WillNotOverflowUnsignedMul", 2},
  {"WillNotOverflowSignedShl", 2},
  {"WillNotOverflowUnsignedShl", 2},
};

FnPred::FnPred(string_view name, vector<Value *> &&args)
  : args(std::move(args)) {
  int idx = -1;
  for (unsigned i = 0; i < sizeof_array(fn_data); ++i) {
    if (fn_data[i].first == name) {
      idx = i;
      break;
    }
  }
  if (idx < 0)
    throw FnPredException("Unknown function: " + string(name));

  auto expected_args = fn_data[idx].second;
  auto actual_args = this->args.size();
  if (actual_args != expected_args)
    throw FnPredException("Expected " + to_string(expected_args) +
                          " parameters for " + string(name) + ", but got " +
                          to_string(actual_args));
  fn = (Fn)idx;
}

void FnPred::print(ostream &os) const {
  os << fn_data[fn].first << '(';
  bool first = true;
  for (auto a : args) {
    if (!first)
      os << ", ";
    first = false;
    os << a->getName();
  }
  os << ')';
}

expr FnPred::mkMustAnalysis(State &s, expr &&e) const {
  bool all_const = true;
  for (auto a : args) {
    all_const &= dynamic_cast<Constant*>(a) != nullptr;
  }

  // analyses are precise when given constant inputs
  if (all_const)
    return std::move(e);

  stringstream ss;
  print(ss);
  string name = ss.str();
  auto var = expr::mkBoolVar(name.c_str());
  s.addPre(var.implies(e));
  return var;
}

expr FnPred::toSMT(State &s) const {
  vector<StateValue> vals;
  for (auto a : args) {
    vals.emplace_back(s[*a]);
  }

  expr r(true);
  for (auto &v : vals) {
    r &= v.non_poison;
  }

  switch (fn) {
  case AddNSW:
    r &= mkMustAnalysis(s, vals[0].value.add_no_soverflow(vals[1].value));
    break;
  case AddNUW:
    r &= mkMustAnalysis(s, vals[0].value.add_no_uoverflow(vals[1].value));
    break;
  case SubNSW:
    r &= mkMustAnalysis(s, vals[0].value.sub_no_soverflow(vals[1].value));
    break;
  case SubNUW:
    r &= mkMustAnalysis(s, vals[0].value.sub_no_uoverflow(vals[1].value));
    break;
  case MulNSW:
    r &= mkMustAnalysis(s, vals[0].value.mul_no_soverflow(vals[1].value));
    break;
  case MulNUW:
    r &= mkMustAnalysis(s, vals[0].value.mul_no_uoverflow(vals[1].value));
    break;
  case ShlNSW:
    r &= mkMustAnalysis(s, vals[0].value.shl_no_soverflow(vals[1].value));
    break;
  case ShlNUW:
    r &= mkMustAnalysis(s, vals[0].value.shl_no_uoverflow(vals[1].value));
    break;
  }
  return r;
}

expr FnPred::getTypeConstraints(const Function &f) const {
  expr r(true);
  for (auto a : args) {
    r &= a->getTypeConstraints(f);
  }
  switch (fn) {
  case AddNSW:
  case AddNUW:
  case SubNSW:
  case SubNUW:
  case MulNSW:
  case MulNUW:
  case ShlNSW:
  case ShlNUW:
    r &= args[0]->getType().enforceIntType() &&
         args[0]->getType() == args[1]->getType();
    break;
  }
  return r;
}

void FnPred::fixupTypes(const Model &m) {
  for (auto a : args) {
    a->fixupTypes(m);
  }
}


void CmpPred::print(ostream &os) const {
  const char *p = nullptr;
  switch (pred) {
  case EQ:  p = " == ";  break;
  case NE:  p = " != ";  break;
  case SLE: p = " <= ";  break;
  case SLT: p = " < ";   break;
  case SGE: p = " >= ";  break;
  case SGT: p = " > ";   break;
  case ULE: p = " u<= "; break;
  case ULT: p = " u< ";  break;
  case UGE: p = " u>= "; break;
  case UGT: p = " u> ";  break;
  }
  lhs.print(os);
  rhs.print(os << p);
}

expr CmpPred::toSMT(State &s) const {
  auto &[a, ap] = s[lhs];
  auto &[b, bp] = s[rhs];
  expr r;
  switch (pred) {
  case EQ:  r = a == b; break;
  case NE:  r = a != b; break;
  case SLE: r = a.sle(b); break;
  case SLT: r = a.slt(b); break;
  case SGE: r = a.sge(b); break;
  case SGT: r = a.sgt(b); break;
  case ULE: r = a.ule(b); break;
  case ULT: r = a.ult(b); break;
  case UGE: r = a.uge(b); break;
  case UGT: r = a.ugt(b); break;
  }
  return { ap && bp && std::move(r) };
}

expr CmpPred::getTypeConstraints(const Function &f) const {
  return lhs.getTypeConstraints(f) &&
         lhs.getType().enforceIntType() &&
         lhs.getType() == rhs.getType();
}

void CmpPred::fixupTypes(const Model &m) {
  lhs.fixupTypes(m);
  rhs.fixupTypes(m);
}

}
