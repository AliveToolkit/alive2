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
  case ExtractValue:
    assert(dynamic_cast<IntConst*>(&rhs));
    assert(dynamic_cast<IntConst&>(rhs).getInt());
    [[fallthrough]];
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
  case FAdd:
  case FSub:
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
  case ExtractValue: str = "extractvalue"; break;
  case FAdd: str = "fadd"; break;
  case FSub: str = "fsub"; break;
  }

  const char *flag = nullptr;
  switch (flags) {
  case None:   flag = " "; break;
  case NSW:    flag = " nsw "; break;
  case NUW:    flag = " nuw "; break;
  case NSWNUW: flag = " nsw nuw "; break;
  case Exact:  flag = " exact "; break;
  }

  os << getName() << " = " << str << flag << lhs << ", " << rhs.getName();
}

static void div_ub(State &s, const expr &a, const expr &b, const expr &ap,
                   const expr &bp, bool sign) {
  auto bits = b.bits();
  s.addUB(bp);
  s.addUB(b != expr::mkUInt(0, bits));
  if (sign)
    s.addUB((ap && a != expr::IntSMin(bits)) || b != expr::mkInt(-1, bits));
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

    auto bits = b.bits();
    not_poison &= b.ult(expr::mkUInt(bits, bits));

    if (flags & NSW)
      not_poison &= a.shl_no_soverflow(b);
    if (flags & NUW)
      not_poison &= a.shl_no_uoverflow(b);
    break;
  }
  case AShr: {
    val = a.ashr(b);

    auto bits = b.bits();
    not_poison &= b.ult(expr::mkUInt(bits, bits));

    if (flags & Exact)
      not_poison &= a.ashr_exact(b);
    break;
  }
  case LShr: {
    val = a.lshr(b);

    auto bits = b.bits();
    not_poison &= b.ult(expr::mkUInt(bits, bits));

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

  case ExtractValue:
  {
    auto structType = lhs.getType().getAsStructType();
    auto idx = *static_cast<IntConst&>(rhs).getInt();
    val = structType->extract(a, idx);
    break;
  }

  case FAdd:
    val = a.fadd(b);
    break;

  case FSub:
    val = a.fsub(b);
    break;
  }

  return { move(val), move(not_poison) };
}

expr BinOp::getTypeConstraints(const Function &f) const {
  expr instrconstr;
  switch (op) {
  case ExtractValue:
  {
    instrconstr = lhs.getType().enforceAggregateType() &&
                  rhs.getType().enforceIntType();

    // TODO: add support for arrays
    if (auto ty = lhs.getType().getAsStructType()) {
      auto idx = *static_cast<IntConst&>(rhs).getInt();
      instrconstr &= ty->numElements().ugt(idx) &&
                     ty->getChild(idx) == getType();
    }
    break;
  }
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
    newval = v;
    break;
  case Ptr2Int:
    newval = s.getMemory().ptr2int(v);
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

  for (auto arg : args) {
    auto &[v, p] = s[*arg];
    // if the value is poison, mask out the arithmetic result
    args_eval.emplace_back(expr::mkIf(p, v, expr::mkUInt(0, v.bits())));
    args_eval.emplace_back(p);
  }

  // impact of the function on the domain of the program
  // TODO: constraint when certain attributes are on
  auto ub_name = fnName + "_ub";
  s.addUB(expr::mkUF(ub_name.c_str(), args_eval, true));

  if (dynamic_cast<VoidType*>(&getType()))
    return {};

  auto poison_name = fnName + "_poison";
  return { expr::mkUF(fnName.c_str(), args_eval, getType().getDummyValue()),
           expr::mkUF(poison_name.c_str(), args_eval, true) };
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
  expr val;
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
  return { expr::mkIf(val, expr::mkUInt(1,1), expr::mkUInt(0,1)), ap && bp };
}

expr ICmp::getTypeConstraints(const Function &f) const {
  expr e = Value::getTypeConstraints() &&
           getType().enforceIntType() &&
           getType().sizeVar() == 1u &&
           a.getType().enforceIntOrPtrOrVectorType() &&
           a.getType() == b.getType();
  if (!defined)
    e &= cond_var().ule(expr::mkUInt(9, 4));
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


void Freeze::print(ostream &os) const {
  os << getName() << " = freeze " << val;
}

StateValue Freeze::toSMT(State &s) const {
  auto &[v, p] = s[val];
  s.resetUndefVars();

  if (p.isTrue())
    return { expr(v), expr(p) };

  auto name = "nondet_" + fresh_id();
  expr nondet = expr::mkVar(name.c_str(), bits());
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
    ptr += expr::mkUInt(sz, bits_offset) * v.sextOrTrunc(bits_offset);
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
  return s.getMemory().load(p, getType().bits(), align);
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
  s.getMemory().store(p, s[val], align);
  return {};
}

expr Store::getTypeConstraints(const Function &f) const {
  return ptr.getType().enforcePtrType();
}

unique_ptr<Instr> Store::dup(const string &suffix) const {
  return make_unique<Store>(ptr, val, align);
}

}
