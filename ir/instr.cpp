// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/instr.h"
#include "ir/function.h"
#include "ir/type.h"
#include "smt/expr.h"
#include "smt/solver.h"
#include "util/compiler.h"

using namespace smt;
using namespace std;

namespace IR {

expr Instr::eqType(const Instr &i) const {
  return getType() == i.getType();
}

expr Instr::getTypeConstraints() const {
  UNREACHABLE();
  return {};
}


BinOp::BinOp(Type &type, string &&name, Value &lhs, Value &rhs, Op op,
             Flags flags)
  : Instr(type, move(name)), lhs(lhs), rhs(rhs), op(op), flags(flags) {
  switch (op) {
  case Add:
  case Sub:
  case Mul:
  case Shl:
    assert((flags & NSWNUW) == flags);
    break;
  case SDiv:
  case UDiv:
  case AShr:
  case LShr:
    assert((flags & Exact) == flags);
    break;
  case FAdd:
  case FSub:
  case FMul:
  case FDiv:
  case FRem:
    assert((flags & NNANNINF) == flags);
    break;
  case SRem:
  case URem:
  case SAdd_Sat:
  case UAdd_Sat:
  case SSub_Sat:
  case USub_Sat:
  case And:
  case Or:
  case Xor:
  case Cttz:
  case Ctlz:
  case SAdd_Overflow:
  case UAdd_Overflow:
  case SSub_Overflow:
  case USub_Overflow:
  case SMul_Overflow:
  case UMul_Overflow:
    assert(flags == 0);
    break;
  }
}

void BinOp::print(ostream &os) const {
  const char *str = nullptr;
  switch (op) {
  case Add:  str = "add"; break;
  case Sub:  str = "sub"; break;
  case Mul:  str = "mul"; break;
  case SDiv: str = "sdiv"; break;
  case UDiv: str = "udiv"; break;
  case SRem: str = "srem"; break;
  case URem: str = "urem"; break;
  case Shl:  str = "shl"; break;
  case AShr: str = "ashr"; break;
  case LShr: str = "lshr"; break;
  case SAdd_Sat: str = "sadd_sat"; break;
  case UAdd_Sat: str = "uadd_sat"; break;
  case SSub_Sat: str = "ssub_sat"; break;
  case USub_Sat: str = "usub_sat"; break;
  case And:  str = "and"; break;
  case Or:   str = "or"; break;
  case Xor:  str = "xor"; break;
  case Cttz: str = "cttz"; break;
  case Ctlz: str = "ctlz"; break;
  case SAdd_Overflow: str = "sadd_overflow"; break;
  case UAdd_Overflow: str = "uadd_overflow"; break;
  case SSub_Overflow: str = "ssub_overflow"; break;
  case USub_Overflow: str = "usub_overflow"; break;
  case SMul_Overflow: str = "smul_overflow"; break;
  case UMul_Overflow: str = "umul_overflow"; break;
  case FAdd: str = "fadd"; break;
  case FSub: str = "fsub"; break;
  case FMul: str = "fmul"; break;
  case FDiv: str = "fdiv"; break;
  case FRem: str = "frem"; break;
  }

  const char *flag = nullptr;
  switch (flags) {
  case None:   flag = " "; break;
  case NSW:    flag = " nsw "; break;
  case NUW:    flag = " nuw "; break;
  case NSWNUW: flag = " nsw nuw "; break;
  case NNAN:   flag = " nnan "; break;
  case NINF:   flag = " ninf "; break;
  case NNANNINF: flag = " nnan ninf "; break;
  case Exact:  flag = " exact "; break;
  }

  os << getName() << " = " << str << flag << lhs << ", " << rhs.getName();
}

static void div_ub(State &s, const expr &a, const expr &b, const expr &ap,
                   const expr &bp, bool sign) {
  auto bits = b.bits();
  s.addUB(bp);
  s.addUB(b != 0);
  if (sign)
    s.addUB((ap && a != expr::IntSMin(bits)) || b != expr::mkInt(-1, bits));
}

static expr fm_poison(const expr &a, const expr &b, const expr &val,
                      BinOp::Flags flags) {
  expr ret = false;
  if (flags & BinOp::NINF)
    ret |= a.isInf() || b.isInf() || val.isInf();
  if (flags & BinOp::NNAN)
    ret |= a.isNaN() || b.isNaN() || val.isNaN();

  return ret;
}

StateValue BinOp::toSMT(State &s) const {
  auto &[a, ap] = s[lhs];
  auto &[b, bp] = s[rhs];
  expr val;
  auto not_poison = ap && bp;

  switch (op) {
  case Add:
    val = a + b;
    if (flags & NSW)
      not_poison &= a.add_no_soverflow(b);
    if (flags & NUW)
      not_poison &= a.add_no_uoverflow(b);
    break;

  case Sub:
    val = a - b;
    if (flags & NSW)
      not_poison &= a.sub_no_soverflow(b);
    if (flags & NUW)
      not_poison &= a.sub_no_uoverflow(b);
    break;

  case Mul:
    val = a * b;
    if (flags & NSW)
      not_poison &= a.mul_no_soverflow(b);
    if (flags & NUW)
      not_poison &= a.mul_no_uoverflow(b);
    break;

  case SDiv:
    val = a.sdiv(b);
    div_ub(s, a, b, ap, bp, true);

    if (flags & Exact)
      not_poison &= a.sdiv_exact(b);
    break;

  case UDiv:
    val = a.udiv(b);
    div_ub(s, a, b, ap, bp, false);

    if (flags & Exact)
      not_poison &= a.udiv_exact(b);
    break;

  case SRem:
    val = a.srem(b);
    div_ub(s, a, b, ap, bp, true);
    break;

  case URem:
    val = a.urem(b);
    div_ub(s, a, b, ap, bp, false);
    break;

  case Shl:
  {
    val = a << b;
    not_poison &= b.ult(b.bits());

    if (flags & NSW)
      not_poison &= a.shl_no_soverflow(b);
    if (flags & NUW)
      not_poison &= a.shl_no_uoverflow(b);
    break;
  }
  case AShr: {
    val = a.ashr(b);
    not_poison &= b.ult(b.bits());

    if (flags & Exact)
      not_poison &= a.ashr_exact(b);
    break;
  }
  case LShr: {
    val = a.lshr(b);
    not_poison &= b.ult(b.bits());

    if (flags & Exact)
      not_poison &= a.lshr_exact(b);
    break;
  }
  case SAdd_Sat:
    val = a.sadd_sat(b);
    break;

  case UAdd_Sat:
    val = a.uadd_sat(b);
    break;

  case SSub_Sat:
    val = a.ssub_sat(b);
    break;

  case USub_Sat:
    val = a.usub_sat(b);
    break;

  case And:
    val = a & b;
    break;

  case Or:
    val = a | b;
    break;

  case Xor:
    val = a ^ b;
    break;

  case Cttz:
    val = a.cttz();
    not_poison &= (b == 0u || a != 0u);
    break;

  case Ctlz:
    val = a.ctlz();
    not_poison &= (b == 0u || a != 0u);
    break;

  case SAdd_Overflow:
    val = a + b;
    val = val.concat((!a.add_no_soverflow(b)).toBVBool());
    break;

  case UAdd_Overflow:
    val = a + b;
    val = val.concat((!a.add_no_uoverflow(b)).toBVBool());
    break;

  case SSub_Overflow:
    val = a - b;
    val = val.concat((!a.sub_no_soverflow(b)).toBVBool());
    break;

  case USub_Overflow:
    val = a - b;
    val = val.concat((!a.sub_no_uoverflow(b)).toBVBool());
    break;

  case SMul_Overflow:
    val = a * b;
    val = val.concat((!a.mul_no_soverflow(b)).toBVBool());
    break;

  case UMul_Overflow:
    val = a * b;
    val = val.concat((!a.mul_no_uoverflow(b)).toBVBool());
    break;

  case FAdd:
    val = a.fadd(b);
    not_poison &= !fm_poison(a, b, val, flags);
    break;

  case FSub:
    val = a.fsub(b);
    not_poison &= !fm_poison(a, b, val, flags);
    break;

  case FMul:
    val = a.fmul(b);
    not_poison &= !fm_poison(a, b, val, flags);
    break;

  case FDiv:
    val = a.fdiv(b);
    not_poison &= !fm_poison(a, b, val, flags);
    break;

  case FRem:
    // TODO; Z3 has no support for LLVM's frem which is actually an fmod
    break;
  }

  return { move(val), move(not_poison) };
}

expr BinOp::getTypeConstraints(const Function &f) const {
  expr instrconstr;
  switch (op) {
  case SAdd_Overflow:
  case UAdd_Overflow:
  case SSub_Overflow:
  case USub_Overflow:
  case SMul_Overflow:
  case UMul_Overflow:
    instrconstr = getType().enforceStructType() &&
                  lhs.getType().enforceIntType() &&
                  lhs.getType() == rhs.getType();

    if (auto ty = getType().getAsStructType()) {
      instrconstr &= ty->numElements() == 2 &&
                     ty->getChild(0) == lhs.getType() &&
                     ty->getChild(1).enforceIntType(1);
    }
    break;
  case Cttz:
  case Ctlz:
    instrconstr = getType().enforceIntOrVectorType() &&
                  getType() == lhs.getType() &&
                  rhs.getType().enforceIntType() &&
                  rhs.getType().sizeVar() == 1u;
    break;
  case FAdd:
  case FSub:
  case FMul:
  case FDiv:
  case FRem:
    instrconstr = getType().enforceFloatType() &&
                  getType() == lhs.getType() &&
                  getType() == rhs.getType();
    break;
  default:
    instrconstr = getType().enforceIntOrVectorType() &&
                  getType() == lhs.getType() &&
                  getType() == rhs.getType();
    break;
  }
  return Value::getTypeConstraints() && move(instrconstr);
}

unique_ptr<Instr> BinOp::dup(const string &suffix) const {
  return make_unique<BinOp>(getType(), getName()+suffix, lhs, rhs, op, flags);
}


void UnaryOp::print(ostream &os) const {
  const char *str = nullptr;
  switch (op) {
  case Copy:        str = ""; break;
  case BitReverse:  str = "bitreverse "; break;
  case BSwap:       str = "bswap "; break;
  case Ctpop:       str = "ctpop "; break;
  case FNeg:        str = "fneg "; break;
  }

  os << getName() << " = " << str << val;
}

StateValue UnaryOp::toSMT(State &s) const {
  auto &[v, vp] = s[val];
  expr newval;

  switch (op) {
  case Copy:
    newval = v;
    break;
  case BitReverse:
    newval = v.bitreverse();
    break;
  case BSwap:
    newval = v.bswap();;
    break;
  case Ctpop:
    newval = v.ctpop();
    break;
  case FNeg:
    newval = v.fneg();
    break;
  }
  return { move(newval), expr(vp) };
}

expr UnaryOp::getTypeConstraints(const Function &f) const {
  expr instrconstr = true;
  switch(op) {
  case BSwap:
    instrconstr = val.getType().sizeVar().urem(expr::mkUInt(16, 8)) == 0;
    [[fallthrough]];
  case BitReverse:
  case Ctpop:
    instrconstr &= getType().enforceIntOrVectorType();
  case Copy:
    break;
  case FNeg:
    instrconstr = getType().enforceFloatType();
    break;
  }

  return Value::getTypeConstraints() &&
         getType() == val.getType() &&
         move(instrconstr);
}

unique_ptr<Instr> UnaryOp::dup(const string &suffix) const {
  return make_unique<UnaryOp>(getType(), getName() + suffix, val, op);
}


void TernaryOp::print(ostream &os) const {
  const char *str = nullptr;
  switch (op) {
  case FShl:
    str = "fshl ";
    break;
  case FShr:
    str = "fshr ";
    break;
  }

  os << getName() << " = " << str << A << ", " << B << ", " << C;
}

StateValue TernaryOp::toSMT(State &s) const {
  auto &[a, ap] = s[A];
  auto &[b, bp] = s[B];
  auto &[c, cp] = s[C];
  expr newval;
  expr not_poison;

  switch (op) {
  case FShl:
    newval = expr::fshl(a, b, c);
    not_poison = ap && bp && cp;
    break;
  case FShr:
    newval = expr::fshr(a, b, c);
    not_poison = ap && bp && cp;
    break;
  }

  return { move(newval), move(not_poison) };
}

expr TernaryOp::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         getType().enforceIntOrVectorType() &&
         getType() == A.getType() &&
         getType() == B.getType() &&
         getType() == C.getType();
}

unique_ptr<Instr> TernaryOp::dup(const string &suffix) const {
  return make_unique<TernaryOp>(getType(), getName() + suffix, A, B, C, op);
}


void ConversionOp::print(ostream &os) const {
  const char *str = nullptr;
  switch (op) {
  case SExt:    str = "sext "; break;
  case ZExt:    str = "zext "; break;
  case Trunc:   str = "trunc "; break;
  case BitCast: str = "bitcast "; break;
  case Ptr2Int: str = "ptrtoint "; break;
  case Int2Ptr: str = "int2ptr "; break;
  }

  os << getName() << " = " << str << val;

  if (auto ty = getType().toString();
      !ty.empty())
    os << " to " << ty;
}

StateValue ConversionOp::toSMT(State &s) const {
  auto &[v, vp] = s[val];
  expr newval;
  auto to_bw = getType().bits();

  switch (op) {
  case SExt:
    newval = v.sext(to_bw - val.bits());
    break;
  case ZExt:
    newval = v.zext(to_bw - val.bits());
    break;
  case Trunc:
    newval = v.trunc(to_bw);
    break;
  case BitCast:
    if ((getType().isIntType() && val.getType().isIntType()) ||
        (getType().isFloatType() && val.getType().isFloatType()) ||
        (getType().isPtrType() && val.getType().isPtrType()))
      newval = v;
    else if (getType().isIntType() && val.getType().isFloatType())
      newval = v.float2BV();
    else if (getType().isFloatType() && val.getType().isIntType())
      newval = v.BV2float(getType().getDummyValue());
    else
      UNREACHABLE();
    break;
  case Ptr2Int:
    newval = s.getMemory().ptr2int(v).zextOrTrunc(getType().bits());
    break;
  case Int2Ptr:
    newval = s.getMemory().int2ptr(v);
    break;
  }
  return { move(newval), expr(vp) };
}

expr ConversionOp::getTypeConstraints(const Function &f) const {
  expr c;
  switch (op) {
  case SExt:
  case ZExt:
    c = getType().enforceIntOrVectorType() &&
        getType().sameType(val.getType()) &&
        val.getType().sizeVar().ult(getType().sizeVar());
    break;
  case Trunc:
    c = getType().enforceIntOrVectorType() &&
        getType().sameType(val.getType()) &&
        getType().sizeVar().ult(val.getType().sizeVar());
    break;
  case BitCast:
    // FIXME: input can only be ptr if result is a ptr as well
    c = getType().enforceIntOrPtrOrVectorType() &&
        getType().sizeVar() == val.getType().sizeVar();
    break;
  case Ptr2Int:
    c = getType().enforceIntType() &&
        val.getType().enforcePtrType();
    break;
  case Int2Ptr:
    c = getType().enforcePtrType() &&
        val.getType().enforceIntType();
    break;
  }
  return Value::getTypeConstraints() && move(c);
}

unique_ptr<Instr> ConversionOp::dup(const string &suffix) const {
  return make_unique<ConversionOp>(getType(), getName() + suffix, val, op);
}


void Select::print(ostream &os) const {
  os << getName() << " = select " << cond << ", " << a << ", " << b;
}

StateValue Select::toSMT(State &s) const {
  auto &[c, cp] = s[cond];
  auto &[av, ap] = s[a];
  auto &[bv, bp] = s[b];
  auto cond = c == 1u;
  return { expr::mkIf(cond, av, bv),
           cp && expr::mkIf(cond, ap, bp) };
}

expr Select::getTypeConstraints(const Function &f) const {
  return cond.getType().enforceIntType() &&
         cond.getType().sizeVar() == 1u &&
         Value::getTypeConstraints() &&
         getType().enforceIntOrPtrOrVectorType() &&
         getType() == a.getType() &&
         getType() == b.getType();
}

unique_ptr<Instr> Select::dup(const string &suffix) const {
  return make_unique<Select>(getType(), getName() + suffix, cond, a, b);
}


void ExtractValue::addIdx(unsigned idx) {
  idxs.emplace_back(idx);
}

void ExtractValue::print(ostream &os) const {
  os << getName() << " = extractvalue " << val;
  for (auto idx : idxs) {
    os << ", " << idx;
  }
}

StateValue ExtractValue::toSMT(State &s) const {
  auto [v, p] = s[val];

  // TODO: add support for array type
  Type *type = &val.getType();
  for (auto idx : idxs) {
    auto st = type->getAsStructType();
    v = st->extract(v, idx);
    type = &st->getChild(idx);
  }

  return { move(v), move(p) };
}

expr ExtractValue::getTypeConstraints(const Function &f) const {
  auto c = Value::getTypeConstraints() &&
           val.getType().enforceAggregateType();

  // TODO: add support for arrays
  Type *type = &val.getType();
  unsigned i = 0;
  for (auto idx : idxs) {
    auto st = type->getAsStructType();
    if (!st) {
      c = false;
      break;
    }
    type = &st->getChild(idx);

    c &= st->numElements().ugt(idx);
    if (++i == idxs.size())
      c &= st->getChild(idx) == getType();
  }
  return c;
}

unique_ptr<Instr> ExtractValue::dup(const string &suffix) const {
  auto ret = make_unique<ExtractValue>(getType(), getName() + suffix, val);
  for (auto idx : idxs) {
    ret->addIdx(idx);
  }
  return ret;
}


void FnCall::addArg(Value &arg) {
  args.emplace_back(&arg);
}

void FnCall::print(ostream &os) const {
  auto type = getType().toString();
  if (!type.empty())
    type += ' ';

  if (!dynamic_cast<VoidType*>(&getType()))
    os << getName() << " = ";

  os << "call " << type << fnName << '(';

  bool first = true;
  for (auto arg : args) {
    if (!first)
      os << ", ";
    os << *arg;
    first = false;
  }
  os << ')';
}

StateValue FnCall::toSMT(State &s) const {
  // TODO: add support for memory
  // TODO: add support for global variables
  vector<expr> args_eval;
  const char *suffix = s.isSource() ? "#src" : "#tgt";

  for (auto arg : args) {
    auto &[v, p] = s[*arg];
    args_eval.emplace_back(v);
    args_eval.emplace_back(p);
  }

  auto add_axiom = [&](string fn, const expr &type,
                       expr (expr::*implies)(const expr &) const) {
    if (!s.isSource())
      return;

    vector<expr> vars_src, vars_tgt;
    unsigned i = 0;
    for (auto &arg : args_eval) {
      auto name = "v" + to_string(i++);
      vars_src.emplace_back(expr::mkVar(name.c_str(), arg));
      name = "v" + to_string(i++);
      vars_tgt.emplace_back(expr::mkVar(name.c_str(), arg));
    }

    auto name_src = fn + "#src";
    auto name_tgt = fn + "#tgt";

    {
      // fns are equal if args are non-poison and equal
      set<expr> vars_set;
      for (unsigned i = 0, e = vars_src.size(); i != e; i += 2) {
        vars_set.insert(vars_src[i]);
      }

      vector<expr> v_src, v_tgt;
      for (unsigned i = 0, e = vars_src.size(); i != e; i += 2) {
        v_src.emplace_back(vars_src[i]);
        v_src.emplace_back(true);
        v_tgt.emplace_back(vars_src[i]);
        v_tgt.emplace_back(true);
      }

      auto fn_src = expr::mkUF(name_src.c_str(), v_src, type);
      auto fn_tgt = expr::mkUF(name_tgt.c_str(), v_tgt, type);
      s.addPre(expr::mkForAll(vars_set, fn_src == fn_tgt));
    }

    {
      // fn src implies fn tgt if source value is poison
      set<expr> vars_set(vars_src.begin(), vars_src.end());
      vars_set.insert(vars_tgt.begin(), vars_tgt.end());

      expr cond(false);
      for (unsigned i = 0, e = vars_src.size(); i != e; i += 2) {
        cond |= !vars_src[i + 1];
      }

      auto fn_src = expr::mkUF(name_src.c_str(), vars_src, type);
      auto fn_tgt = expr::mkUF(name_tgt.c_str(), vars_tgt, type);
      s.addPre(expr::mkForAll(vars_set,
                              cond.implies((fn_src.*implies)(fn_tgt))));
    }
  };


  // impact of the function on the domain of the program
  // TODO: constraint when certain attributes are on
  auto ub_name = fnName + "_ub" + suffix;
  s.addUB(expr::mkUF(ub_name.c_str(), args_eval, false));

  add_axiom(fnName + "_ub", false, &expr::implies);

  if (dynamic_cast<VoidType*>(&getType()))
    return {};

  expr ret_type = getType().getDummyValue();
  add_axiom(fnName, ret_type, &expr::operator==);
  add_axiom(fnName + "_poison", false, &expr::implies);

  auto val_name = fnName + suffix;
  auto poison_name = fnName + "_poison" + suffix;
  return { expr::mkUF(val_name.c_str(), args_eval, ret_type),
           expr::mkUF(poison_name.c_str(), args_eval, false) };
}

expr FnCall::getTypeConstraints(const Function &f) const {
  // TODO : also need to name each arg type smt var uniquely
  return Value::getTypeConstraints();
}

unique_ptr<Instr> FnCall::dup(const string &suffix) const {
  auto r = make_unique<FnCall>(getType(), getName() + suffix, string(fnName));
  r->args = args;
  return r;
}


ICmp::ICmp(Type &type, string &&name, Cond cond, Value &a, Value &b)
  : Instr(type, move(name)), a(a), b(b), cond(cond), defined(cond != Any) {
  if (!defined)
    cond_name = getName() + "_cond" + fresh_id();
}

expr ICmp::cond_var() const {
  return defined ? expr::mkUInt(cond, 4) : expr::mkVar(cond_name.c_str(), 4);
}

void ICmp::print(ostream &os) const {
  const char *condtxt = nullptr;
  switch (cond) {
  case EQ:  condtxt = "eq "; break;
  case NE:  condtxt = "ne "; break;
  case SLE: condtxt = "sle "; break;
  case SLT: condtxt = "slt "; break;
  case SGE: condtxt = "sge "; break;
  case SGT: condtxt = "sgt "; break;
  case ULE: condtxt = "ule "; break;
  case ULT: condtxt = "ult "; break;
  case UGE: condtxt = "uge "; break;
  case UGT: condtxt = "ugt "; break;
  case Any: condtxt = ""; break;
  }
  os << getName() << " = icmp " << condtxt << a << ", " << b.getName();
}

StateValue ICmp::toSMT(State &s) const {
  auto &[av, ap] = s[a];
  auto &[bv, bp] = s[b];
  expr val, non_poison = ap && bp;

  if (a.getType().isPtrType()) {
    Pointer lhs(s.getMemory(), av);
    Pointer rhs(s.getMemory(), bv);
    StateValue cmp;

    switch (cond) {
    case EQ:  cmp = { lhs == rhs, true }; break;
    case NE:  cmp = { lhs != rhs, true }; break;
    case SLE: cmp = lhs.sle(rhs); break;
    case SLT: cmp = lhs.slt(rhs); break;
    case SGE: cmp = lhs.sge(rhs); break;
    case SGT: cmp = lhs.sgt(rhs); break;
    case ULE: cmp = lhs.ule(rhs); break;
    case ULT: cmp = lhs.ult(rhs); break;
    case UGE: cmp = lhs.uge(rhs); break;
    case UGT: cmp = lhs.ugt(rhs); break;
    case Any:
      UNREACHABLE();
    }
    val = move(cmp.value);
    non_poison &= move(cmp.non_poison);

  } else {  // integer comparison
    switch (cond) {
    case EQ:  val = av == bv; break;
    case NE:  val = av != bv; break;
    case SLE: val = av.sle(bv); break;
    case SLT: val = av.slt(bv); break;
    case SGE: val = av.sge(bv); break;
    case SGT: val = av.sgt(bv); break;
    case ULE: val = av.ule(bv); break;
    case ULT: val = av.ult(bv); break;
    case UGE: val = av.uge(bv); break;
    case UGT: val = av.ugt(bv); break;
    case Any:
      UNREACHABLE();
    }
  }
  return { val.toBVBool(), move(non_poison) };
}

expr ICmp::getTypeConstraints(const Function &f) const {
  expr e = Value::getTypeConstraints() &&
           getType().enforceIntType() &&
           getType().sizeVar() == 1u &&
           a.getType().enforceIntOrPtrOrVectorType() &&
           a.getType() == b.getType();
  if (!defined)
    e &= cond_var().ule(9);
  return e;
}

void ICmp::fixupTypes(const Model &m) {
  Value::fixupTypes(m);
  if (!defined)
    cond = (Cond)m.getUInt(cond_var());
}

expr ICmp::eqType(const Instr &i) const {
  expr eq = Instr::eqType(i);
  if (auto rhs = dynamic_cast<const ICmp*>(&i)) {
    if (!defined || !rhs->defined) {
      eq &= cond_var() == rhs->cond_var();
    }
  }
  return eq;
}

unique_ptr<Instr> ICmp::dup(const string &suffix) const {
  return make_unique<ICmp>(getType(), getName() + suffix, cond, a, b);
}


FCmp::FCmp(Type &type, string &&name, Cond cond, Value &a, Value &b)
  : Instr(type, move(name)), a(a), b(b), cond(cond) {}

void FCmp::print(ostream &os) const {
  const char *condtxt = nullptr;
  switch (cond) {
  case OEQ:   condtxt = "oeq "; break;
  case OGT:   condtxt = "ogt "; break;
  case OGE:   condtxt = "oge "; break;
  case OLT:   condtxt = "olt "; break;
  case OLE:   condtxt = "ole "; break;
  case ONE:   condtxt = "one "; break;
  case ORD:   condtxt = "ord "; break;
  case UEQ:   condtxt = "ueq "; break;
  case UGT:   condtxt = "ugt "; break;
  case UGE:   condtxt = "uge "; break;
  case ULT:   condtxt = "ult "; break;
  case ULE:   condtxt = "ule "; break;
  case UNE:   condtxt = "une "; break;
  case UNO:   condtxt = "uno "; break;
  }
  os << getName() << " = fcmp " << condtxt << a << ", " << b.getName();
}

StateValue FCmp::toSMT(State &s) const {
  auto &[av, ap] = s[a];
  auto &[bv, bp] = s[b];
  expr val;
  switch (cond) {
  case OEQ:   val = av.foeq(bv); break;
  case OGT:   val = av.fogt(bv); break;
  case OGE:   val = av.foge(bv); break;
  case OLT:   val = av.folt(bv); break;
  case OLE:   val = av.fole(bv); break;
  case ONE:   val = av.fone(bv); break;
  case ORD:   val = av.ford(bv); break;
  case UEQ:   val = av.fueq(bv); break;
  case UGT:   val = av.fugt(bv); break;
  case UGE:   val = av.fuge(bv); break;
  case ULT:   val = av.fult(bv); break;
  case ULE:   val = av.fule(bv); break;
  case UNE:   val = av.fune(bv); break;
  case UNO:   val = av.funo(bv); break;
  }
  return { val.toBVBool(), ap && bp };
}

expr FCmp::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         getType().enforceIntType() &&
         getType().sizeVar() == 1u &&
         a.getType().enforceFloatType() &&
         a.getType() == b.getType();
}

unique_ptr<Instr> FCmp::dup(const string &suffix) const {
  return make_unique<FCmp>(getType(), getName() + suffix, cond, a, b);
}


void Freeze::print(ostream &os) const {
  os << getName() << " = freeze " << val;
}

StateValue Freeze::toSMT(State &s) const {
  auto &[v, p] = s[val];
  s.resetUndefVars();

  if (p.isTrue())
    return { expr(v), expr(p) };

  auto name = "nondet_" + fresh_id();
  expr nondet = expr::mkVar(name.c_str(), getType().getDummyValue());
  s.addQuantVar(nondet);

  return { expr::mkIf(p, v, move(nondet)), true };
}

expr Freeze::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         getType() == val.getType();
}

unique_ptr<Instr> Freeze::dup(const string &suffix) const {
  return make_unique<Freeze>(getType(), getName() + suffix, val);
}


void Phi::addValue(Value &val, string &&BB_name) {
  values.emplace_back(val, move(BB_name));
}

void Phi::print(ostream &os) const {
  os << getName() << " = phi ";
  auto t = getType().toString();
  if (!t.empty())
    os << t << ' ';

  bool first = true;
  for (auto &[val, bb] : values) {
    if (!first)
      os << ", ";
    os << "[ " << val.getName() << ", " << bb << " ]";
    first = false;
  }
}

StateValue Phi::toSMT(State &s) const {
  StateValue ret;
  bool first = true;

  for (auto &[val, bb] : values) {
    auto pre = s.jumpCondFrom(s.getFn().getBB(bb));
    if (!pre) // jump from unreachable BB
      continue;

    auto v = s[val];
    if (first) {
      ret = v;
      first = false;
    } else {
      ret = StateValue::mkIf(*pre, v, ret);
    }
  }
  return ret;
}

expr Phi::getTypeConstraints(const Function &f) const {
  auto c = Value::getTypeConstraints();
  for (auto &[val, bb] : values) {
    (void)bb;
    c &= val.getType() == getType();
  }
  return c;
}

unique_ptr<Instr> Phi::dup(const string &suffix) const {
  auto phi = make_unique<Phi>(getType(), getName() + suffix);
  for (auto &[val, bb] : values) {
    phi->addValue(val, string(bb));
  }
  return phi;
}


const BasicBlock& JumpInstr::target_iterator::operator*() const {
  if (auto br = dynamic_cast<Branch*>(instr))
    return idx == 0 ? br->getTrue() : *br->getFalse();

  if (auto sw = dynamic_cast<Switch*>(instr))
    return idx == 0 ? sw->getDefault() : sw->getTarget(idx-1).second;

  UNREACHABLE();
}

JumpInstr::target_iterator JumpInstr::it_helper::end() const {
  unsigned idx;
  if (auto br = dynamic_cast<Branch*>(instr)) {
    idx = br->getFalse() ? 2 : 1;
  } else if (auto sw = dynamic_cast<Switch*>(instr)) {
    idx = sw->getNumTargets() + 1;
  } else {
    UNREACHABLE();
  }
  return { instr, idx };
}


void Branch::print(ostream &os) const {
  os << "br ";
  if (cond)
    os << *cond << ", ";
  os << "label " << dst_true.getName();
  if (dst_false)
    os << ", label " << dst_false->getName();
}

StateValue Branch::toSMT(State &s) const {
  if (cond) {
    s.addCondJump(s[*cond], dst_true, *dst_false);
  } else {
    s.addJump(dst_true);
  }
  return {};
}

expr Branch::getTypeConstraints(const Function &f) const {
  return cond->getType().enforceIntType() &&
         cond->getType().sizeVar() == 1u;
}

unique_ptr<Instr> Branch::dup(const string &suffix) const {
  if (dst_false)
    return make_unique<Branch>(*cond, dst_true, *dst_false);
  return make_unique<Branch>(dst_true);
}


void Switch::addTarget(Value &val, const BasicBlock &target) {
  targets.emplace_back(val, target);
}

void Switch::print(ostream &os) const {
  os << "switch " << value << ", label " << default_target.getName() << " [\n";
  for (auto &[val, target] : targets) {
    os << "    " << val << ", label " << target.getName() << '\n';
  }
  os << "  ]";
}

StateValue Switch::toSMT(State &s) const {
  auto val = s[value];
  expr default_cond(true);

  for (auto &[value_cond, bb] : targets) {
    auto target = s[value_cond];
    assert(target.non_poison.isTrue());
    s.addJump({ val.value == target.value, expr(val.non_poison) }, bb);
    default_cond &= val.value != target.value;
  }

  s.addJump({ move(default_cond), move(val.non_poison) }, default_target);
  s.addUB(false);
  return {};
}

expr Switch::getTypeConstraints(const Function &f) const {
  expr typ = value.getType().enforceIntType();
  for (auto &p : targets) {
    typ &= p.first.getType().enforceIntType();
  }
  return typ;
}

unique_ptr<Instr> Switch::dup(const string &suffix) const {
  auto sw = make_unique<Switch>(value, default_target);
  for (auto &[value_cond, bb] : targets) {
    sw->addTarget(value_cond, bb);
  }
  return sw;
}


void Return::print(ostream &os) const {
  os << "ret " << val;
}

StateValue Return::toSMT(State &s) const {
  s.addReturn(s[val]);
  return {};
}

expr Return::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         getType() == val.getType() &&
         f.getType() == getType();
}

unique_ptr<Instr> Return::dup(const string &suffix) const {
  return make_unique<Return>(getType(), val);
}


void Assume::print(ostream &os) const {
  os << (if_non_poison ? "assume_non_poison " : "assume ") << cond;
}

StateValue Assume::toSMT(State &s) const {
  auto &[v, np] = s[cond];
  if (if_non_poison)
    s.addUB(np.implies(v != 0));
  else
    s.addUB(np && v != 0);
  return {};
}

expr Assume::getTypeConstraints(const Function &f) const {
  return cond.getType().enforceIntType();
}

unique_ptr<Instr> Assume::dup(const string &suffix) const {
  return make_unique<Assume>(cond, if_non_poison);
}


void Alloc::print(std::ostream &os) const {
  os << getName() << " = alloca " << size << ", align " << align;
}

StateValue Alloc::toSMT(State &s) const {
  auto &[sz, np] = s[size];
  s.addUB(np);
  return { s.getMemory().alloc(sz, align, true), true };
}

expr Alloc::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         getType().enforcePtrType() &&
         size.getType().enforceIntType();
}

unique_ptr<Instr> Alloc::dup(const string &suffix) const {
  return make_unique<Alloc>(getType(), getName() + suffix, size, align);
}


void Free::print(std::ostream &os) const {
  os << "free " << ptr;
}

StateValue Free::toSMT(State &s) const {
  auto &[p, np] = s[ptr];
  s.addUB(np);
  s.getMemory().free(p);
  return {};
}

expr Free::getTypeConstraints(const Function &f) const {
  return ptr.getType().enforcePtrType();
}

unique_ptr<Instr> Free::dup(const string &suffix) const {
  return make_unique<Free>(ptr);
}


void GEP::addIdx(unsigned obj_size, Value &idx) {
  idxs.emplace_back(obj_size, idx);
}

void GEP::print(std::ostream &os) const {
  os << getName() << " = gep ";
  if (inbounds)
    os << "inbounds ";
  os << ptr;

  for (auto &[sz, idx] : idxs) {
    os << ", " << sz << " x " << idx;
  }
}

StateValue GEP::toSMT(State &s) const {
  auto [val, non_poison] = s[ptr];
  unsigned bits_offset = s.getMemory().bitsOffset();

  Pointer ptr(s.getMemory(), move(val));
  if (inbounds)
    non_poison &= ptr.inbounds();

  for (auto &[sz, idx] : idxs) {
    auto &[v, np] = s[idx];
    auto inc = expr::mkUInt(sz, bits_offset) * v.sextOrTrunc(bits_offset);

    if (inbounds)
      non_poison &= ptr.add_no_overflow(inc);

    ptr += inc;
    non_poison &= np;

    if (inbounds)
      non_poison &= ptr.inbounds();
  }
  return { ptr.release(), move(non_poison) };
}

expr GEP::getTypeConstraints(const Function &f) const {
  auto c = getType() == ptr.getType() &&
           ptr.getType().enforcePtrType();
  for (auto &[sz, idx] : idxs) {
    (void)sz;
    c &= idx.getType().enforceIntType();
  }
  return c;
}

unique_ptr<Instr> GEP::dup(const string &suffix) const {
  auto dup = make_unique<GEP>(getType(), getName() + suffix, ptr, inbounds);
  for (auto &[sz, idx] : idxs) {
    dup->addIdx(sz, idx);
  }
  return dup;
}


void Load::print(std::ostream &os) const {
  os << getName() << " = load " << getType() << ", " << ptr
     << ", align " << align;
}

StateValue Load::toSMT(State &s) const {
  auto &[p, np] = s[ptr];
  s.addUB(np);
  return s.getMemory().load(p, getType(), align);
}

expr Load::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         ptr.getType().enforcePtrType();
}

unique_ptr<Instr> Load::dup(const string &suffix) const {
  return make_unique<Load>(getType(), getName() + suffix, ptr, align);
}


void Store::print(std::ostream &os) const {
  os << "store " << val << ", " << ptr << ", align " << align;
}

StateValue Store::toSMT(State &s) const {
  auto &[p, np] = s[ptr];
  s.addUB(np);
  s.getMemory().store(p, s[val], val.getType(), align);
  return {};
}

expr Store::getTypeConstraints(const Function &f) const {
  return ptr.getType().enforcePtrType();
}

unique_ptr<Instr> Store::dup(const string &suffix) const {
  return make_unique<Store>(ptr, val, align);
}

}
