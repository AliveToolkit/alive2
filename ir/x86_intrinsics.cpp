#include "ir/x86_intrinsics.h"
#include "smt/expr.h"

using namespace smt;
using namespace std;

namespace IR {
vector<Value *> FakeShuffle::operands() const {
  return {v1, v2, mask};
}

bool FakeShuffle::propagatesPoison() const {
  return false;
}

bool FakeShuffle::hasSideEffects() const {
  return false;
}

void FakeShuffle::rauw(const Value &what, Value &with) {
  RAUW(v1);
  RAUW(v2);
  RAUW(mask);
}

void FakeShuffle::print(ostream &os) const {
  os << getName() << " = fakesv " << *v1 << ", " << *v2 << ", " << *mask;
}

StateValue FakeShuffle::toSMT(State &s) const {
  auto vty =
      static_cast<const VectorType *>(v1->getType().getAsAggregateType());
  auto mty = mask->getType().getAsAggregateType();
  auto sz = vty->numElementsConst();
  vector<StateValue> vals;

  for (unsigned i = 0, e = mty->numElementsConst(); i != e; ++i) {
    auto [m_v, m_p] = mty->extract(s[*mask], i);
    expr bound = expr::mkUInt(sz, m_v);
    expr idx = m_v.urem(bound);
    auto [v1v, v1p] = vty->extract(s[*v1], idx);
    auto [v2v, v2p] = vty->extract(s[*v2], idx);
    expr v = expr::mkIf(m_v.ult(bound), v1v, v2v);
    expr np = expr::mkIf(m_v.ult(bound), v1p, v2p);
    expr inbounds = m_v.ult(expr::mkUInt(vty->numElementsConst() * 2, m_v));

    vals.emplace_back(std::move(v), inbounds && np);
  }

  return getType().getAsAggregateType()->aggregateVals(vals);
}

expr FakeShuffle::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         getType().enforceVectorTypeSameChildTy(v1->getType()) &&
         getType().getAsAggregateType()->numElements() ==
             mask->getType().getAsAggregateType()->numElements() &&
         v1->getType().enforceVectorType() && v1->getType() == v2->getType() &&
         mask->getType().enforceVectorType();
}

unique_ptr<Instr> FakeShuffle::dup(Function &f, const string &suffix) const {
  return make_unique<FakeShuffle>(getType(), getName() + suffix, *v1, *v2,
                                  *mask);
}

vector<Value *> X86IntrinBinOp::operands() const {
  return {a, b};
}

std::pair<unsigned, unsigned> X86IntrinBinOp::shape_op0[] = {
#define PROCESS(NAME, A, B, C, D, E, F) std::make_pair(C, D),
#include "x86_intrinsics_binop.inc"
#undef PROCESS
};

std::pair<unsigned, unsigned> X86IntrinBinOp::shape_op1[] = {
#define PROCESS(NAME, A, B, C, D, E, F) std::make_pair(E, F),
#include "x86_intrinsics_binop.inc"
#undef PROCESS
};

std::pair<unsigned, unsigned> X86IntrinBinOp::shape_ret[] = {
#define PROCESS(NAME, A, B, C, D, E, F) std::make_pair(A, B),
#include "x86_intrinsics_binop.inc"
#undef PROCESS
};

unsigned X86IntrinBinOp::ret_width[] = {
#define PROCESS(NAME, A, B, C, D, E, F) A *B,
#include "x86_intrinsics_binop.inc"
#undef PROCESS
};

bool X86IntrinBinOp::propagatesPoison() const {
  return true;
}

bool X86IntrinBinOp::hasSideEffects() const {
  return false;
}

void X86IntrinBinOp::rauw(const Value &what, Value &with) {
  RAUW(a);
  RAUW(b);
}

string X86IntrinBinOp::getOpName(Op op) {
  switch (op) {
#define PROCESS(NAME, A, B, C, D, E, F)                                        \
  case NAME:                                                                   \
    return #NAME;
#include "x86_intrinsics_binop.inc"
#undef PROCESS
  }
  UNREACHABLE();
}

void X86IntrinBinOp::print(ostream &os) const {
  os << getName() << " = " << getOpName(op) << " " << *a << ", " << *b;
}

StateValue X86IntrinBinOp::toSMT(State &s) const {
  auto rty = getType().getAsAggregateType();
  auto aty = a->getType().getAsAggregateType();
  auto bty = b->getType().getAsAggregateType();
  auto &av = s[*a];
  auto &bv = s[*b];

  switch (op) {
  // shift by one variable
  case x86_sse2_psrl_w:
  case x86_sse2_psrl_d:
  case x86_sse2_psrl_q:
  case x86_avx2_psrl_w:
  case x86_avx2_psrl_d:
  case x86_avx2_psrl_q:
  case x86_avx512_psrl_w_512:
  case x86_avx512_psrl_d_512:
  case x86_avx512_psrl_q_512:
  case x86_sse2_psra_w:
  case x86_sse2_psra_d:
  case x86_avx2_psra_w:
  case x86_avx2_psra_d:
  case x86_avx512_psra_q_128:
  case x86_avx512_psra_q_256:
  case x86_avx512_psra_w_512:
  case x86_avx512_psra_d_512:
  case x86_avx512_psra_q_512:
  case x86_sse2_psll_w:
  case x86_sse2_psll_d:
  case x86_sse2_psll_q:
  case x86_avx2_psll_w:
  case x86_avx2_psll_d:
  case x86_avx2_psll_q:
  case x86_avx512_psll_w_512:
  case x86_avx512_psll_d_512:
  case x86_avx512_psll_q_512: {
    vector<StateValue> vals;
    unsigned elem_bw = bty->getChild(0).bits();

    expr shift_np = true;
    expr shift_v;
    // extract lower 64 bits from b
    for (unsigned i = 0, e = 64 / elem_bw; i != e; ++i) {
      StateValue vv = bty->extract(bv, i);
      shift_v = (i == 0) ? vv.value : vv.value.concat(shift_v);
      // if any elements in lower 64 bits is poison, the result is poison
      shift_np &= vv.non_poison;
    }
    function<expr(const expr &, const expr &)> fn;
    switch (op) {
    case x86_sse2_psrl_w:
    case x86_sse2_psrl_d:
    case x86_sse2_psrl_q:
    case x86_avx2_psrl_w:
    case x86_avx2_psrl_d:
    case x86_avx2_psrl_q:
    case x86_avx512_psrl_w_512:
    case x86_avx512_psrl_d_512:
    case x86_avx512_psrl_q_512:
      fn = [&](auto a, auto b) -> expr {
        return expr::mkIf(shift_v.uge(expr::mkUInt(elem_bw, 64)),
                          expr::mkUInt(0, elem_bw), a.lshr(b));
      };
      break;
    case x86_sse2_psra_w:
    case x86_sse2_psra_d:
    case x86_avx2_psra_w:
    case x86_avx2_psra_d:
    case x86_avx512_psra_q_128:
    case x86_avx512_psra_q_256:
    case x86_avx512_psra_w_512:
    case x86_avx512_psra_d_512:
    case x86_avx512_psra_q_512:
      fn = [&](auto a, auto b) -> expr {
        return expr::mkIf(shift_v.uge(expr::mkUInt(elem_bw, 64)),
                          expr::mkIf(a.isNegative(), expr::mkUInt(-1, elem_bw),
                                     expr::mkUInt(0, elem_bw)),
                          a.ashr(b));
      };
      break;
    case x86_sse2_psll_w:
    case x86_sse2_psll_d:
    case x86_sse2_psll_q:
    case x86_avx2_psll_w:
    case x86_avx2_psll_d:
    case x86_avx2_psll_q:
    case x86_avx512_psll_w_512:
    case x86_avx512_psll_d_512:
    case x86_avx512_psll_q_512:
      fn = [&](auto a, auto b) -> expr {
        return expr::mkIf(shift_v.uge(expr::mkUInt(elem_bw, 64)),
                          expr::mkUInt(0, elem_bw), a << b);
      };
      break;
    default:
      UNREACHABLE();
    }
    for (unsigned i = 0, e = aty->numElementsConst(); i != e; ++i) {
      auto ai = aty->extract(av, i);
      expr shift = fn(ai.value, shift_v.trunc(elem_bw));
      vals.emplace_back(std::move(shift), shift_np && ai.non_poison);
    }
    return rty->aggregateVals(vals);
  }
  // vertical
  case x86_sse2_pavg_w:
  case x86_sse2_pavg_b:
  case x86_avx2_pavg_w:
  case x86_avx2_pavg_b:
  case x86_avx512_pavg_w_512:
  case x86_avx512_pavg_b_512:
  case x86_ssse3_psign_b_128:
  case x86_ssse3_psign_w_128:
  case x86_ssse3_psign_d_128:
  case x86_avx2_psign_b:
  case x86_avx2_psign_w:
  case x86_avx2_psign_d:
  case x86_avx2_psrlv_d:
  case x86_avx2_psrlv_d_256:
  case x86_avx2_psrlv_q:
  case x86_avx2_psrlv_q_256:
  case x86_avx512_psrlv_d_512:
  case x86_avx512_psrlv_q_512:
  case x86_avx512_psrlv_w_128:
  case x86_avx512_psrlv_w_256:
  case x86_avx512_psrlv_w_512:
  case x86_avx2_psrav_d:
  case x86_avx2_psrav_d_256:
  case x86_avx512_psrav_d_512:
  case x86_avx512_psrav_q_128:
  case x86_avx512_psrav_q_256:
  case x86_avx512_psrav_q_512:
  case x86_avx512_psrav_w_128:
  case x86_avx512_psrav_w_256:
  case x86_avx512_psrav_w_512:
  case x86_avx2_psllv_d:
  case x86_avx2_psllv_d_256:
  case x86_avx2_psllv_q:
  case x86_avx2_psllv_q_256:
  case x86_avx512_psllv_d_512:
  case x86_avx512_psllv_q_512:
  case x86_avx512_psllv_w_128:
  case x86_avx512_psllv_w_256:
  case x86_avx512_psllv_w_512:
  case x86_sse2_pmulh_w:
  case x86_avx2_pmulh_w:
  case x86_avx512_pmulh_w_512:
  case x86_sse2_pmulhu_w:
  case x86_avx2_pmulhu_w:
  case x86_avx512_pmulhu_w_512: {
    vector<StateValue> vals;
    function<expr(const expr &, const expr &)> fn;
    switch (op) {
    case x86_sse2_pavg_w:
    case x86_sse2_pavg_b:
    case x86_avx2_pavg_w:
    case x86_avx2_pavg_b:
    case x86_avx512_pavg_w_512:
    case x86_avx512_pavg_b_512:
      fn = [&](auto a, auto b) -> expr {
        unsigned bw = a.bits();
        return (a.zext(1) + b.zext(1) + expr::mkUInt(1, bw + 1))
            .lshr(expr::mkUInt(1, bw + 1))
            .trunc(bw);
      };
      break;
    case x86_ssse3_psign_b_128:
    case x86_ssse3_psign_w_128:
    case x86_ssse3_psign_d_128:
    case x86_avx2_psign_b:
    case x86_avx2_psign_w:
    case x86_avx2_psign_d:
      fn = [&](auto a, auto b) -> expr {
        return expr::mkIf(
            b.isZero(), b,
            expr::mkIf(b.isNegative(), expr::mkUInt(0, a.bits()) - a, a));
      };
      break;
    case x86_avx2_psrlv_d:
    case x86_avx2_psrlv_d_256:
    case x86_avx2_psrlv_q:
    case x86_avx2_psrlv_q_256:
    case x86_avx512_psrlv_d_512:
    case x86_avx512_psrlv_q_512:
    case x86_avx512_psrlv_w_128:
    case x86_avx512_psrlv_w_256:
    case x86_avx512_psrlv_w_512:
      fn = [&](auto a, auto b) -> expr {
        unsigned bw = a.bits();
        return expr::mkIf(b.uge(expr::mkUInt(bw, bw)), expr::mkUInt(0, bw),
                          a.lshr(b));
      };
      break;
    case x86_avx2_psrav_d:
    case x86_avx2_psrav_d_256:
    case x86_avx512_psrav_d_512:
    case x86_avx512_psrav_q_128:
    case x86_avx512_psrav_q_256:
    case x86_avx512_psrav_q_512:
    case x86_avx512_psrav_w_128:
    case x86_avx512_psrav_w_256:
    case x86_avx512_psrav_w_512:
      fn = [&](auto a, auto b) -> expr {
        unsigned bw = a.bits();
        return expr::mkIf(b.uge(expr::mkUInt(bw, bw)),
                          expr::mkIf(a.isNegative(), expr::mkUInt(-1, bw),
                                     expr::mkUInt(0, bw)),
                          a.ashr(b));
      };
      break;
    case x86_avx2_psllv_d:
    case x86_avx2_psllv_d_256:
    case x86_avx2_psllv_q:
    case x86_avx2_psllv_q_256:
    case x86_avx512_psllv_d_512:
    case x86_avx512_psllv_q_512:
    case x86_avx512_psllv_w_128:
    case x86_avx512_psllv_w_256:
    case x86_avx512_psllv_w_512:
      fn = [&](auto a, auto b) -> expr {
        unsigned bw = a.bits();
        return expr::mkIf(b.uge(expr::mkUInt(bw, bw)), expr::mkUInt(0, bw),
                          a << b);
      };
      break;
    case x86_sse2_pmulh_w:
    case x86_avx2_pmulh_w:
    case x86_avx512_pmulh_w_512:
      fn = [&](auto a, auto b) -> expr {
        expr mul = a.sext(16) * b.sext(16);
        return mul.extract(31, 16);
      };
      break;
    case x86_sse2_pmulhu_w:
    case x86_avx2_pmulhu_w:
    case x86_avx512_pmulhu_w_512:
      fn = [&](auto a, auto b) -> expr {
        expr mul = a.zext(16) * b.zext(16);
        return mul.extract(31, 16);
      };
      break;
    default:
      UNREACHABLE();
    }
    for (unsigned i = 0, e = rty->numElementsConst(); i != e; ++i) {
      auto ai = aty->extract(av, i);
      auto bi = bty->extract(bv, i);
      vals.emplace_back(fn(ai.value, bi.value), ai.non_poison && bi.non_poison);
    }
    return rty->aggregateVals(vals);
  }
  // pshuf.b
  case x86_ssse3_pshuf_b_128:
  case x86_avx2_pshuf_b:
  case x86_avx512_pshuf_b_512: {
    auto avty = static_cast<const VectorType *>(aty);
    vector<StateValue> vals;
    unsigned laneCount = shape_ret[op].first;
    for (unsigned i = 0; i != laneCount; ++i) {
      auto [b, bp] = bty->extract(bv, i);
      expr id = (b & expr::mkUInt(0x0F, 8)) + (expr::mkUInt(i & 0x30, 8));
      auto [r, rp] = avty->extract(av, id);
      auto ai = expr::mkIf(b.extract(7, 7) == 0, r, expr::mkUInt(0, 8));
      vals.emplace_back(std::move(ai), bp && rp);
    }
    return rty->aggregateVals(vals);
  }
  // horizontal
  case x86_ssse3_phadd_w_128:
  case x86_ssse3_phadd_d_128:
  case x86_ssse3_phadd_sw_128:
  case x86_avx2_phadd_w:
  case x86_avx2_phadd_d:
  case x86_avx2_phadd_sw:
  case x86_ssse3_phsub_w_128:
  case x86_ssse3_phsub_d_128:
  case x86_ssse3_phsub_sw_128:
  case x86_avx2_phsub_w:
  case x86_avx2_phsub_d:
  case x86_avx2_phsub_sw: {
    vector<StateValue> vals;
    unsigned laneCount = shape_ret[op].first;
    unsigned groupsize = 128 / shape_ret[op].second;
    function<expr(const expr &, const expr &)> fn;
    switch (op) {
    case x86_ssse3_phadd_w_128:
    case x86_ssse3_phadd_d_128:
    case x86_avx2_phadd_w:
    case x86_avx2_phadd_d:
      fn = [&](auto a, auto b) -> expr { return a + b; };
      break;
    case x86_ssse3_phadd_sw_128:
    case x86_avx2_phadd_sw:
      fn = [&](auto a, auto b) -> expr { return a.sadd_sat(b); };
      break;
    case x86_ssse3_phsub_w_128:
    case x86_ssse3_phsub_d_128:
    case x86_avx2_phsub_w:
    case x86_avx2_phsub_d:
      fn = [&](auto a, auto b) -> expr { return a - b; };
      break;
    case x86_ssse3_phsub_sw_128:
    case x86_avx2_phsub_sw:
      fn = [&](auto a, auto b) -> expr { return a.ssub_sat(b); };
      break;
    default:
      UNREACHABLE();
    }
    for (unsigned j = 0; j != laneCount / groupsize; j++) {
      for (unsigned i = 0; i != groupsize; i += 2) {
        auto [a1, p1] = aty->extract(av, j * groupsize + i);
        auto [a2, p2] = aty->extract(av, j * groupsize + i + 1);
        vals.emplace_back(fn(a1, a2), p1 && p2);
      }
      for (unsigned i = 0; i != groupsize; i += 2) {
        auto [b1, p1] = aty->extract(bv, j * groupsize + i);
        auto [b2, p2] = aty->extract(bv, j * groupsize + i + 1);
        vals.emplace_back(fn(b1, b2), p1 && p2);
      }
    }
    return rty->aggregateVals(vals);
  }
  case x86_sse2_psrli_w:
  case x86_sse2_psrli_d:
  case x86_sse2_psrli_q:
  case x86_avx2_psrli_w:
  case x86_avx2_psrli_d:
  case x86_avx2_psrli_q:
  case x86_avx512_psrli_w_512:
  case x86_avx512_psrli_d_512:
  case x86_avx512_psrli_q_512:
  case x86_sse2_psrai_w:
  case x86_sse2_psrai_d:
  case x86_avx2_psrai_w:
  case x86_avx2_psrai_d:
  case x86_avx512_psrai_w_512:
  case x86_avx512_psrai_d_512:
  case x86_avx512_psrai_q_128:
  case x86_avx512_psrai_q_256:
  case x86_avx512_psrai_q_512:
  case x86_sse2_pslli_w:
  case x86_sse2_pslli_d:
  case x86_sse2_pslli_q:
  case x86_avx2_pslli_w:
  case x86_avx2_pslli_d:
  case x86_avx2_pslli_q:
  case x86_avx512_pslli_w_512:
  case x86_avx512_pslli_d_512:
  case x86_avx512_pslli_q_512: {
    vector<StateValue> vals;
    function<expr(const expr &, const expr &)> fn;
    switch (op) {
    case x86_sse2_psrai_w:
    case x86_sse2_psrai_d:
    case x86_avx2_psrai_w:
    case x86_avx2_psrai_d:
    case x86_avx512_psrai_w_512:
    case x86_avx512_psrai_d_512:
    case x86_avx512_psrai_q_128:
    case x86_avx512_psrai_q_256:
    case x86_avx512_psrai_q_512:
      fn = [&](auto a, auto b) -> expr {
        unsigned sz_a = a.bits();
        expr check = b.uge(expr::mkUInt(sz_a, 32));
        expr outbounds = expr::mkIf(a.isNegative(), expr::mkInt(-1, sz_a),
                                    expr::mkUInt(0, sz_a));
        expr inbounds = a.ashr(b.zextOrTrunc(sz_a));
        return expr::mkIf(std::move(check), std::move(outbounds),
                          std::move(inbounds));
      };
      break;
    case x86_sse2_psrli_w:
    case x86_sse2_psrli_d:
    case x86_sse2_psrli_q:
    case x86_avx2_psrli_w:
    case x86_avx2_psrli_d:
    case x86_avx2_psrli_q:
    case x86_avx512_psrli_w_512:
    case x86_avx512_psrli_d_512:
    case x86_avx512_psrli_q_512:
      fn = [&](auto a, auto b) -> expr {
        unsigned sz_a = a.bits();
        expr check = b.uge(expr::mkUInt(sz_a, 32));
        expr outbounds = expr::mkUInt(0, sz_a);
        expr inbounds = a.lshr(b.zextOrTrunc(sz_a));
        return expr::mkIf(std::move(check), std::move(outbounds),
                          std::move(inbounds));
      };
      break;
    case x86_sse2_pslli_w:
    case x86_sse2_pslli_d:
    case x86_sse2_pslli_q:
    case x86_avx2_pslli_w:
    case x86_avx2_pslli_d:
    case x86_avx2_pslli_q:
    case x86_avx512_pslli_w_512:
    case x86_avx512_pslli_d_512:
    case x86_avx512_pslli_q_512:
      fn = [&](auto a, auto b) -> expr {
        unsigned sz_a = a.bits();
        expr check = b.uge(expr::mkUInt(sz_a, 32));
        expr outbounds = expr::mkUInt(0, sz_a);
        expr inbounds = a << b.zextOrTrunc(sz_a);
        return expr::mkIf(std::move(check), std::move(outbounds),
                          std::move(inbounds));
      };
      break;
    default:
      UNREACHABLE();
    }
    for (unsigned i = 0, e = rty->numElementsConst(); i != e; ++i) {
      auto ai = aty->extract(av, i);
      vals.emplace_back(fn(ai.value, bv.value), ai.non_poison && bv.non_poison);
    }
    return rty->aggregateVals(vals);
  }
  case x86_sse2_pmadd_wd:
  case x86_avx2_pmadd_wd:
  case x86_avx512_pmaddw_d_512:
  case x86_ssse3_pmadd_ub_sw_128:
  case x86_avx2_pmadd_ub_sw:
  case x86_avx512_pmaddubs_w_512: {
    vector<StateValue> vals;
    for (unsigned i = 0, e = shape_ret[op].first; i != e; ++i) {
      auto [a1, a1p] = aty->extract(av, i * 2);
      auto [a2, a2p] = aty->extract(av, i * 2 + 1);
      auto [b1, b1p] = bty->extract(bv, i * 2);
      auto [b2, b2p] = bty->extract(bv, i * 2 + 1);

      auto np = a1p && a2p && b1p && b2p;

      if (op == x86_sse2_pmadd_wd || op == x86_avx2_pmadd_wd ||
          op == x86_avx512_pmaddw_d_512) {
        expr v = a1.sext(16) * b1.sext(16) + a2.sext(16) * b2.sext(16);
        vals.emplace_back(std::move(v), std::move(np));
      } else {
        expr v = (a1.zext(8) * b1.sext(8)).sadd_sat(a2.zext(8) * b2.sext(8));
        vals.emplace_back(std::move(v), std::move(np));
      }
    }
    return rty->aggregateVals(vals);
  }
  case x86_sse2_packsswb_128:
  case x86_avx2_packsswb:
  case x86_avx512_packsswb_512:
  case x86_sse2_packuswb_128:
  case x86_avx2_packuswb:
  case x86_avx512_packuswb_512:
  case x86_sse2_packssdw_128:
  case x86_avx2_packssdw:
  case x86_avx512_packssdw_512:
  case x86_sse41_packusdw:
  case x86_avx2_packusdw:
  case x86_avx512_packusdw_512: {
    vector<StateValue> vals;
    function<expr(const expr &)> fn;
    if (op == x86_sse2_packsswb_128 || op == x86_avx2_packsswb ||
        op == x86_avx512_packsswb_512 || op == x86_sse2_packssdw_128 ||
        op == x86_avx2_packssdw || op == x86_avx512_packssdw_512) {
      fn = [&](auto a) -> expr {
        unsigned bw = a.bits() / 2;
        auto min = expr::IntSMin(bw);
        auto max = expr::IntSMax(bw);
        return expr::mkIf(a.sle(min.sext(bw)), min,
                          expr::mkIf(a.sge(max.sext(bw)), max, a.trunc(bw)));
      };
    } else {
      fn = [&](auto a) -> expr {
        unsigned bw = a.bits() / 2;
        auto max = expr::IntUMax(bw);
        auto zero = expr::mkUInt(0, bw);
        return expr::mkIf(a.sle(zero.zext(bw)), zero,
                          expr::mkIf(a.sge(max.zext(bw)), max, a.trunc(bw)));
      };
    }

    unsigned groupsize = 128 / shape_op1[op].second;
    unsigned laneCount = shape_op1[op].first;
    for (unsigned j = 0; j != laneCount / groupsize; j++) {
      for (unsigned i = 0; i != groupsize; i++) {
        auto [a1, p1] = aty->extract(av, j * groupsize + i);
        vals.emplace_back(fn(std::move(a1)), std::move(p1));
      }
      for (unsigned i = 0; i != groupsize; i++) {
        auto [b1, p1] = aty->extract(bv, j * groupsize + i);
        vals.emplace_back(fn(std::move(b1)), std::move(p1));
      }
    }
    return rty->aggregateVals(vals);
  }
  case x86_sse2_psad_bw:
  case x86_avx2_psad_bw:
  case x86_avx512_psad_bw_512: {
    unsigned ngroup = shape_ret[op].first;
    vector<StateValue> vals;
    for (unsigned j = 0; j < ngroup; ++j) {
      expr np = true;
      expr v;
      for (unsigned i = 0; i < 8; ++i) {
        auto [a, ap] = aty->extract(av, 8 * j + i);
        auto [b, bp] = bty->extract(bv, 8 * j + i);
        np = np && ap && bp;
        if (i == 0)
          v = (a.zext(8) - b.zext(8)).abs();
        else
          v = v + (a.zext(8) - b.zext(8)).abs();
      }
      vals.emplace_back(v.zext(48), std::move(np));
    }
    return rty->aggregateVals(vals);
  }
  }
  UNREACHABLE();
}

expr X86IntrinBinOp::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         (shape_op0[op].first != 1
              ? a->getType().enforceVectorType([this](auto &ty) {
                  return ty.enforceIntType(shape_op0[op].second);
                }) &&
                    a->getType().getAsAggregateType()->numElements() ==
                        shape_op0[op].first
              : a->getType().enforceIntType(shape_op0[op].second)) &&
         (shape_op1[op].first != 1
              ? b->getType().enforceVectorType([this](auto &ty) {
                  return ty.enforceIntType(shape_op1[op].second);
                }) &&
                    b->getType().getAsAggregateType()->numElements() ==
                        shape_op1[op].first
              : b->getType().enforceIntType(shape_op1[op].second)) &&
         (shape_ret[op].first != 1
              ? getType().enforceVectorType([this](auto &ty) {
                  return ty.enforceIntType(shape_ret[op].second);
                }) &&
                    getType().getAsAggregateType()->numElements() ==
                        shape_ret[op].first
              : getType().enforceIntType(shape_ret[op].second));
}

unique_ptr<Instr> X86IntrinBinOp::dup(Function &f, const string &suffix) const {
  return make_unique<X86IntrinBinOp>(getType(), getName() + suffix, *a, *b, op);
}

std::pair<unsigned, unsigned> X86IntrinTerOp::shape_op0[] = {
#define PROCESS(NAME, A, B, C, D, E, F, G, H) std::make_pair(C, D),
#include "x86_intrinsics_terop.inc"
#undef PROCESS
};

std::pair<unsigned, unsigned> X86IntrinTerOp::shape_op1[] = {
#define PROCESS(NAME, A, B, C, D, E, F, G, H) std::make_pair(E, F),
#include "x86_intrinsics_terop.inc"
#undef PROCESS
};

std::pair<unsigned, unsigned> X86IntrinTerOp::shape_op2[] = {
#define PROCESS(NAME, A, B, C, D, E, F, G, H) std::make_pair(G, H),
#include "x86_intrinsics_terop.inc"
#undef PROCESS
};

std::pair<unsigned, unsigned> X86IntrinTerOp::shape_ret[] = {
#define PROCESS(NAME, A, B, C, D, E, F, G, H) std::make_pair(A, B),
#include "x86_intrinsics_terop.inc"
#undef PROCESS
};

unsigned X86IntrinTerOp::ret_width[] = {
#define PROCESS(NAME, A, B, C, D, E, F, G, H) A *B,
#include "x86_intrinsics_terop.inc"
#undef PROCESS
};

string X86IntrinTerOp::getOpName(Op op) {
  switch (op) {
#define PROCESS(NAME, A, B, C, D, E, F, G, H)                                  \
  case NAME:                                                                   \
    return #NAME;
#include "x86_intrinsics_terop.inc"
#undef PROCESS
  }
  UNREACHABLE();
}

void X86IntrinTerOp::print(ostream &os) const {
  os << getName() << " = " << getOpName(op) << " " << *a << ", " << *b;
}

StateValue X86IntrinTerOp::toSMT(State &s) const {
  auto rty = getType().getAsAggregateType();
  auto aty = a->getType().getAsAggregateType();
  auto bty = b->getType().getAsAggregateType();
  auto cty = c->getType().getAsAggregateType();
  auto &av = s[*a];
  auto &bv = s[*b];
  auto &cv = s[*c];

  switch (op) {
  case x86_avx2_pblendvb: {
    vector<StateValue> vals;

    for (int i = 0; i < 32; ++i) {
      auto [a, ap] = aty->extract(av, i);
      auto [b, bp] = bty->extract(bv, i);
      auto [c, cp] = cty->extract(cv, i);
      auto v = expr::mkIf(c.extract(7, 7) == 0, a, b);
      vals.emplace_back(std::move(v), ap && bp && cp);
    }
    return rty->aggregateVals(vals);
  }
  }
  UNREACHABLE();
}

expr X86IntrinTerOp::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         (shape_op0[op].first != 1
              ? a->getType().enforceVectorType([this](auto &ty) {
                  return ty.enforceIntType(shape_op0[op].second);
                }) &&
                    a->getType().getAsAggregateType()->numElements() ==
                        shape_op0[op].first
              : a->getType().enforceIntType(shape_op0[op].second)) &&
         (shape_op1[op].first != 1
              ? b->getType().enforceVectorType([this](auto &ty) {
                  return ty.enforceIntType(shape_op1[op].second);
                }) &&
                    b->getType().getAsAggregateType()->numElements() ==
                        shape_op1[op].first
              : b->getType().enforceIntType(shape_op1[op].second)) &&
         (shape_op2[op].first != 1
              ? b->getType().enforceVectorType([this](auto &ty) {
                  return ty.enforceIntType(shape_op2[op].second);
                }) &&
                    b->getType().getAsAggregateType()->numElements() ==
                        shape_op2[op].first
              : b->getType().enforceIntType(shape_op2[op].second)) &&
         (shape_ret[op].first != 1
              ? getType().enforceVectorType([this](auto &ty) {
                  return ty.enforceIntType(shape_ret[op].second);
                }) &&
                    getType().getAsAggregateType()->numElements() ==
                        shape_ret[op].first
              : getType().enforceIntType(shape_ret[op].second));
}

unique_ptr<Instr> X86IntrinTerOp::dup(Function &f, const string &suffix) const {
  return make_unique<X86IntrinTerOp>(getType(), getName() + suffix, *a, *b, *c,
                                     op);
}

vector<Value *> X86IntrinTerOp::operands() const {
  return {a, b, c};
}

bool X86IntrinTerOp::propagatesPoison() const {
  return true;
}

bool X86IntrinTerOp::hasSideEffects() const {
  return false;
}

void X86IntrinTerOp::rauw(const Value &what, Value &with) {
  RAUW(a);
  RAUW(b);
  RAUW(c);
}
} // namespace IR
