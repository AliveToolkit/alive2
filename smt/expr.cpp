// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/expr.h"
#include "smt/exprs.h"
#include "smt/ctx.h"
#include "util/compiler.h"
#include <algorithm>
#include <bit>
#include <cassert>
#include <climits>
#include <limits>
#include <memory>
#include <string>
#include <z3.h>

#define DEBUG_Z3_RC 0
#define WARN_MISSING_FOLDS 0

#if DEBUG_Z3_RC || WARN_MISSING_FOLDS
# include <iostream>
#endif

using namespace smt;
using namespace std;
using namespace util;

// helpers to check if all input arguments are non-null
#define C(...)                                                                 \
  if (!isValid() || !expr::allValid( __VA_ARGS__))                             \
    return {}

#define C2(...)                                                                \
  if (!expr::allValid(__VA_ARGS__))                                            \
    return {}


static Z3_sort mkBVSort(unsigned bits) {
  assert(bits > 0);
  return Z3_mk_bv_sort(smt::ctx(), bits);
}

static Z3_ast mkVar(const char *name, Z3_sort sort) {
  return Z3_mk_const(smt::ctx(), Z3_mk_string_symbol(smt::ctx(), name), sort);
}

static expr simplify_const(expr &&e) { return e.simplifyNoTimeout(); }

template <typename... Exprs>
static expr simplify_const(expr &&e, const expr &input,
                           const Exprs &... inputs) {
  if (input.isConst())
    return simplify_const(std::move(e), inputs...);

#if WARN_MISSING_FOLDS
  if (!e.isConst() && e.simplify().isConst()) {
    cout << "\n[WARN] missing fold: " << e << "\n->\n" << e.simplify() << '\n';
  }
#endif
  return std::move(e);
}

static bool is_power2(const expr &e, unsigned &log) {
  if (e.isZero() || !(e & (e - expr::mkUInt(1, e))).isZero())
    return false;

  for (unsigned i = 0, bits = e.bits(); i < bits; ++i) {
    if (e.extract(i, i).isAllOnes()) {
      log = i;
      return true;
    }
  }
  UNREACHABLE();
}


namespace smt {

expr::expr(Z3_ast ast) noexcept : ptr((uintptr_t)ast) {
  static_assert(sizeof(Z3_ast) == sizeof(uintptr_t));
  assert(isZ3Ast() && isValid());
  incRef();
#if DEBUG_Z3_RC
  cout << "[Z3RC] newObj " << ast << ' ' << *this << '\n';
#endif
}

bool expr::isZ3Ast() const {
  return true;
  //return (ptr & 1) == 0;
}

Z3_ast expr::ast() const {
  assert(isValid());

  if (isZ3Ast())
    return (Z3_ast)ptr;

  assert(0 && "TODO");
  return 0;
}

expr::expr(const expr &other) noexcept {
  if (!other.isValid()) {
    ptr = 0;
  } else if (other.isZ3Ast()) {
    ptr = other.ptr;
    incRef();
  } else {
    assert(0 && "TODO");
  }
}

expr::~expr() noexcept {
  if (isValid()) {
    if (isZ3Ast()) {
      decRef();
    } else {
      assert(0 && "TODO");
    }
  }
}

void expr::incRef() {
  assert(isZ3Ast() && isValid());
  Z3_inc_ref(ctx(), ast());
#if DEBUG_Z3_RC
  cerr << "[Z3RC] incRef " << ast() << '\n';
#endif
}

void expr::decRef() {
  assert(isZ3Ast() && isValid());
  Z3_dec_ref(ctx(), ast());
#if DEBUG_Z3_RC
  cerr << "[Z3RC] decRef " << ast() << '\n';
#endif
}

void expr::operator=(expr &&other) {
  this->~expr();
  ptr = 0;
  swap(ptr, other.ptr);
}

void expr::operator=(const expr &other) {
  this->~expr();
  if (!other.isValid()) {
    ptr = 0;
  } else if (other.isZ3Ast()) {
    ptr = other.ptr;
    incRef();
  } else {
    assert(0 && "TODO");
  }
}

Z3_sort expr::sort() const {
  return Z3_get_sort(ctx(), ast());
}

Z3_decl expr::decl() const {
  auto app = isApp();
  return app ? Z3_get_app_decl(ctx(), app) : nullptr;
}

Z3_app expr::isApp() const {
  C();
  auto z3_ast = ast();
  if (Z3_is_app(ctx(), z3_ast))
    return Z3_to_app(ctx(), z3_ast);
  return nullptr;
}

Z3_app expr::isAppOf(int app_type) const {
  auto app = isApp();
  if (!app)
    return nullptr;
  auto decl = Z3_get_app_decl(ctx(), app);
  return Z3_get_decl_kind(ctx(), decl) == app_type ? app : nullptr;
}

Z3_ast expr::mkTrue() {
  return Z3_mk_true(ctx());
}

Z3_ast expr::mkFalse() {
  return Z3_mk_false(ctx());
}

expr expr::mkUInt(uint64_t n, Z3_sort sort) {
  return Z3_mk_unsigned_int64(ctx(), n, sort);
}

expr expr::mkUInt(uint64_t n, unsigned bits) {
  return bits ? mkUInt(n, mkBVSort(bits)) : expr();
}

expr expr::mkUInt(uint64_t n, const expr &type) {
  C2(type);
  return mkUInt(n, type.sort());
}

expr expr::mkInt(int64_t n, Z3_sort sort) {
  return Z3_mk_int64(ctx(), n, sort);
}

expr expr::mkInt(int64_t n, unsigned bits) {
  return bits ? mkInt(n, mkBVSort(bits)) : expr();
}

expr expr::mkInt(int64_t n, const expr &type) {
  C2(type);
  return mkInt(n, type.sort());
}

expr expr::mkInt(const char *n, unsigned bits) {
  return bits ? Z3_mk_numeral(ctx(), n, mkBVSort(bits)) : expr();
}

expr expr::mkFloat(double n, const expr &type) {
  C2(type);
  return Z3_mk_fpa_numeral_double(ctx(), n, type.sort());
}

expr expr::mkHalf(float n) {
  return Z3_mk_fpa_numeral_float(ctx(), n, Z3_mk_fpa_sort_half(ctx()));
}

static Z3_sort mk_bfloat_sort() {
  return Z3_mk_fpa_sort(ctx(), 8, 8);
}

expr expr::mkBFloat(float n) {
  return Z3_mk_fpa_numeral_float(ctx(), n, mk_bfloat_sort());
}

expr expr::mkFloat(float n) {
  return Z3_mk_fpa_numeral_float(ctx(), n, Z3_mk_fpa_sort_single(ctx()));
}

expr expr::mkDouble(double n) {
  return Z3_mk_fpa_numeral_double(ctx(), n, Z3_mk_fpa_sort_double(ctx()));
}

expr expr::mkQuad(double n) {
  return Z3_mk_fpa_numeral_double(ctx(), n, Z3_mk_fpa_sort_quadruple(ctx()));
}

expr expr::mkNaN(const expr &type) {
  C2(type);
  return Z3_mk_fpa_nan(ctx(), type.sort());
}

expr expr::mkNumber(const char *n, const expr &type) {
  C2(type);
  return Z3_mk_numeral(ctx(), n, type.sort());
}

expr expr::mkConst(Z3_decl decl) {
  return Z3_mk_app(ctx(), decl, 0, {});
}

bool expr::isUnOp(expr &a, int z3op) const {
  if (auto app = isAppOf(z3op)) {
    a = Z3_get_app_arg(ctx(), app, 0);
    return true;
  }
  return false;
}

bool expr::isBinOp(expr &a, expr &b, int z3op) const {
  if (auto app = isAppOf(z3op)) {
    if (Z3_get_app_num_args(ctx(), app) != 2)
      return false;
    a = Z3_get_app_arg(ctx(), app, 0);
    b = Z3_get_app_arg(ctx(), app, 1);
    return true;
  }
  return false;
}

bool expr::isTernaryOp(expr &a, expr &b, expr &c, int z3op) const {
  if (auto app = isAppOf(z3op)) {
    a = Z3_get_app_arg(ctx(), app, 0);
    b = Z3_get_app_arg(ctx(), app, 1);
    c = Z3_get_app_arg(ctx(), app, 2);
    return true;
  }
  return false;
}

expr expr::mkVar(const char *name, const expr &type) {
  C2(type);
  return ::mkVar(name, type.sort());
}

expr expr::mkVar(const char *name, unsigned bits, bool fresh) {
  auto sort = mkBVSort(bits);
  return fresh ? Z3_mk_fresh_const(ctx(), name, sort)
               : ::mkVar(name, sort);
}

expr expr::mkBoolVar(const char *name) {
  return ::mkVar(name, Z3_mk_bool_sort(ctx()));
}

expr expr::mkHalfVar(const char *name) {
  return ::mkVar(name, Z3_mk_fpa_sort_half(ctx()));
}

expr expr::mkBFloatVar(const char *name) {
  return ::mkVar(name, mk_bfloat_sort());
}

expr expr::mkFloatVar(const char *name) {
  return ::mkVar(name, Z3_mk_fpa_sort_single(ctx()));
}

expr expr::mkDoubleVar(const char *name) {
  return ::mkVar(name, Z3_mk_fpa_sort_double(ctx()));
}

expr expr::mkQuadVar(const char *name) {
  return ::mkVar(name, Z3_mk_fpa_sort_quadruple(ctx()));
}

expr expr::mkFreshVar(const char *prefix, const expr &type) {
  C2(type);
  return Z3_mk_fresh_const(ctx(), prefix, type.sort());
}

expr expr::some(const expr &type) {
  return type.isBool() ? expr(false) : mkNumber("3", type);
}

expr expr::IntSMin(unsigned bits) {
  if (bits == 0)
    return {};
  expr v = mkUInt(1, 1);
  if (bits > 1)
    v = v.concat(mkUInt(0, bits - 1));
  return v;
}

expr expr::IntSMax(unsigned bits) {
  if (bits == 0)
    return {};
  expr v = mkUInt(0, 1);
  if (bits > 1)
    v = v.concat(mkInt(-1, bits - 1));
  return v;
}

expr expr::IntUMax(unsigned bits) {
  return mkInt(-1, bits);
}

bool expr::eq(const expr &rhs) const {
  C(rhs);
  return ast() == rhs();
}

bool expr::isConst() const {
  C();
  return Z3_is_numeral_ast(ctx(), ast()) ||
         Z3_get_bool_value(ctx(), ast()) != Z3_L_UNDEF;
}

bool expr::isVar() const {
  C();
  if (auto app = isApp())
    return !isConst() && Z3_get_app_num_args(ctx(), app) == 0;
  return false;
}

bool expr::isBV() const {
  C();
  return Z3_get_sort_kind(ctx(), sort()) == Z3_BV_SORT;
}

bool expr::isBool() const {
  C();
  return Z3_get_sort_kind(ctx(), sort()) == Z3_BOOL_SORT;
}

bool expr::isFloat() const {
  C();
  return Z3_get_sort_kind(ctx(), sort()) == Z3_FLOATING_POINT_SORT;
}

bool expr::isTrue() const {
  C();
  return Z3_get_bool_value(ctx(), ast()) == Z3_L_TRUE;
}

bool expr::isFalse() const {
  C();
  return Z3_get_bool_value(ctx(), ast()) == Z3_L_FALSE;
}

bool expr::isZero() const {
  uint64_t n;
  return isUInt(n) && n == 0;
}

bool expr::isOne() const {
  uint64_t n;
  return isUInt(n) && n == 1;
}

bool expr::isAllOnes() const {
  C();
  return eq(mkInt(-1, sort()));
}

bool expr::isSMin() const {
  return eq(IntSMin(bits()));
}

bool expr::isSMax() const {
  return eq(IntSMax(bits()));
}

expr expr::isNegative() const {
  return sign() == 1;
}

unsigned expr::bits() const {
  C();
  return Z3_get_bv_sort_size(ctx(), sort());
}

bool expr::isUInt(uint64_t &n) const {
  C();
  return Z3_get_numeral_uint64(ctx(), ast(), &n);
}

bool expr::isInt(int64_t &n) const {
  C();
  auto bw = bits();
  if (bw > 64 || !Z3_get_numeral_int64(ctx(), ast(), &n))
    return false;

  if (bw < 64)
    n = (int64_t)((uint64_t)n << (64 - bw)) >> (64 - bw);
  return true;
}

bool expr::isSameTypeOf(const expr &other) const {
  C(other);
  return sort() == other.sort();
}

bool expr::isEq(expr &lhs, expr &rhs) const {
  return isBinOp(lhs, rhs, Z3_OP_EQ);
}

bool expr::isSLE(expr &lhs, expr &rhs) const {
  return isBinOp(lhs, rhs, Z3_OP_SLEQ);
}

bool expr::isULE(expr &lhs, expr &rhs) const {
  return isBinOp(lhs, rhs, Z3_OP_ULEQ);
}

bool expr::isIf(expr &cond, expr &then, expr &els) const {
  return isTernaryOp(cond, then, els, Z3_OP_ITE);
}

bool expr::isConcat(expr &a, expr &b) const {
  if (auto app = isAppOf(Z3_OP_CONCAT)) {
    auto nargs = Z3_get_app_num_args(ctx(), app);
    assert(nargs >= 2);
    a = Z3_get_app_arg(ctx(), app, 0);
    b = Z3_get_app_arg(ctx(), app, 1);
    for (unsigned i = 2; i < nargs; ++i) {
      b = b.concat(Z3_get_app_arg(ctx(), app, i));
    }
    return true;
  }
  return false;
}

bool expr::isExtract(expr &e, unsigned &high, unsigned &low) const {
  if (auto app = isAppOf(Z3_OP_EXTRACT)) {
    e = Z3_get_app_arg(ctx(), app, 0);
    auto d = decl();
    high = Z3_get_decl_int_parameter(ctx(), d, 0);
    low = Z3_get_decl_int_parameter(ctx(), d, 1);
    return true;
  }
  return false;
}

bool expr::isSignExt(expr &val) const {
  return isUnOp(val, Z3_OP_SIGN_EXT);
}

bool expr::isAnd(expr &a, expr &b) const {
  return isBinOp(a, b, Z3_OP_AND);
}

bool expr::isNot(expr &neg) const {
  return isUnOp(neg, Z3_OP_NOT);
}

bool expr::isAdd(expr &a, expr &b) const {
  return isBinOp(a, b, Z3_OP_BADD);
}

bool expr::isURem(expr &a, expr &b) const {
  return isBinOp(a, b, Z3_OP_BUREM);
}

bool expr::isBasePlusOffset(expr &base, uint64_t &offset) const {
  expr a, b;
  if (isAdd(a, b)) {
    if (a.isUInt(offset)) {
      base = b;
      return true;
    }
    if (b.isUInt(offset)) {
      base = a;
      return true;
    }
  }
  base = *this;
  offset = 0;
  return true;
}

bool expr::isConstArray(expr &val) const {
  return isUnOp(val, Z3_OP_CONST_ARRAY);
}

bool expr::isStore(expr &array, expr &idx, expr &val) const {
  return isTernaryOp(array, idx, val, Z3_OP_STORE);
}

bool expr::isLoad(expr &array, expr &idx) const {
  return isBinOp(array, idx, Z3_OP_SELECT);
}

bool expr::isLambda(expr &body) const {
  C();
  if (Z3_is_lambda(ctx(), ast())) {
    assert(Z3_get_quantifier_num_bound(ctx(), ast()) == 1);
    body = Z3_get_quantifier_body(ctx(), ast());
    return true;
  }
  return false;
}
expr expr::lambdaIdxType() const {
  C();
  assert(Z3_get_quantifier_num_bound(ctx(), ast()) == 1);
  return ::mkVar("sort", Z3_get_quantifier_bound_sort(ctx(), ast(), 0));
}

bool expr::isFPAdd(expr &rounding, expr &lhs, expr &rhs) const {
  return isTernaryOp(rounding, lhs, rhs, Z3_OP_FPA_ADD);
}

bool expr::isFPSub(expr &rounding, expr &lhs, expr &rhs) const {
  return isTernaryOp(rounding, lhs, rhs, Z3_OP_FPA_SUB);
}

bool expr::isFPMul(expr &rounding, expr &lhs, expr &rhs) const {
  return isTernaryOp(rounding, lhs, rhs, Z3_OP_FPA_MUL);
}

bool expr::isFPDiv(expr &rounding, expr &lhs, expr &rhs) const {
  return isTernaryOp(rounding, lhs, rhs, Z3_OP_FPA_DIV);
}

bool expr::isFPNeg(expr &val) const {
  if (isBV()) {
    // extract(signbit, signbit) ^ 1).concat(extract(signbit-1, 0))
    expr sign, rest, a, b, val, val2;
    unsigned high, high2, low;
    auto check_not = [&](const expr &a, const expr &b) {
      unsigned l;
      return a.isExtract(val, high, l) && b.isAllOnes() && b.bits() == 1;
    };
    return isConcat(sign, rest) &&
           sign.isBinOp(a, b, Z3_OP_BXOR) &&
           (check_not(a, b) || check_not(b, a)) &&
           rest.isExtract(val2, high2, low) &&
           low == 0 && high2 == high - 1 &&
           val.eq(val2);
  }
  return isUnOp(val, Z3_OP_FPA_NEG);
}

bool expr::isFAbs(expr &val) const {
  return isUnOp(val, Z3_OP_FPA_ABS);
}

bool expr::isIsFPZero() const {
  if (isBV()) {
    // extract(bits()-2, 0) == 0
    expr lhs, rhs, v;
    unsigned high, low;
    return isEq(lhs, rhs) &&
           lhs.isExtract(v, high, low) &&
           high == bits()-2 && low == 0 &&
           rhs.isZero();
  }
  return isAppOf(Z3_OP_FPA_IS_ZERO);
}

bool expr::isNaNCheck(expr &fp) const {
  if (auto app = isAppOf(Z3_OP_FPA_IS_NAN)) {
    fp = Z3_get_app_arg(ctx(), app, 0);
    return true;
  }
  return false;
}

bool expr::isfloat2BV(expr &fp) const {
  return isUnOp(fp, Z3_OP_FPA_TO_IEEE_BV);
}

unsigned expr::min_leading_zeros() const {
  expr a, b;
  uint64_t n;
  if (isConcat(a, b)) {
    return a.min_leading_zeros();
  } else if (isUInt(n)) {
    return countl_zero(n) - (64 - bits());
  }
  return 0;
}

unsigned expr::min_trailing_ones() const {
  uint64_t n;
  if (isUInt(n))
    return countr_one(n);
  return 0;
}

expr expr::binop_commutative(const expr &rhs,
                             Z3_ast (*op)(Z3_context, Z3_ast, Z3_ast),
                             expr (expr::*expr_op)(const expr &) const,
                             bool (expr::*identity)() const,
                             bool (expr::*absorvent)() const,
                             int z3_app) const {
  if ((this->*absorvent)() || (rhs.*identity)())
    return *this;

  if ((rhs.*absorvent)() || (this->*identity)())
    return rhs;

  if (z3_app) {
    if (isConst()) {
      auto app = rhs.isAppOf(z3_app);
      if (app && Z3_get_app_num_args(ctx(), app) == 2) {
        expr app_a = Z3_get_app_arg(ctx(), app, 0);
        expr app_b = Z3_get_app_arg(ctx(), app, 1);
        if (app_a.isConst())
          return ((this->*expr_op)(app_a).*expr_op)(app_b);
        if (app_b.isConst())
          return ((this->*expr_op)(app_b).*expr_op)(app_a);
      }
    }
    else if (rhs.isConst())
      return rhs.binop_commutative(*this, op, expr_op, identity, absorvent,
                                   z3_app);
  }

  return binop_commutative(rhs, op);
}

expr expr::binop_commutative(const expr &rhs,
                             Z3_ast(*op)(Z3_context, Z3_ast, Z3_ast)) const {
  assert(!isValid() || !rhs.isValid() || sort() == rhs.sort());
  auto cmp = *this < rhs;
  return (cmp ? *this : rhs).binop_fold(cmp ? rhs : *this, op);
}

expr expr::binop_fold(const expr &rhs,
                      Z3_ast(*op)(Z3_context, Z3_ast, Z3_ast)) const {
  C(rhs);
  return simplify_const(op(ctx(), ast(), rhs()), *this, rhs);
}

expr expr::unop_fold(Z3_ast(*op)(Z3_context, Z3_ast)) const {
  C();
  return simplify_const(op(ctx(), ast()), *this);
}

#define binopc(op, exprop, z3app, identity, absorvent)                         \
  binop_commutative(rhs, op, &expr::exprop, &expr::identity, &expr::absorvent, \
                    z3app)

expr expr::operator+(const expr &rhs) const {
  expr a, b;
  // (concat x const) + rhs -> (concat const+rhs) if no overflow happens
  if (rhs.isConst() && isConcat(a, b) && b.isConst()) {
    auto rhs2 = rhs.trunc(b.bits());
    if (rhs2.zextOrTrunc(rhs.bits()).eq(rhs) &&
        b.add_no_uoverflow(rhs2).isTrue())
      return a.concat(b + rhs2);
  }
  return binopc(Z3_mk_bvadd, operator+, Z3_OP_BADD, isZero, alwaysFalse);
}

expr expr::operator-(const expr &rhs) const {
  C();
  if (eq(rhs))
    return mkUInt(0, sort());
  return *this + mkInt(-1, sort()) * rhs;
}

expr expr::operator*(const expr &rhs) const {
  unsigned power;
  if (rhs.isConst() && is_power2(rhs, power))
    return *this << mkUInt(power, sort());
  if (isConst() && is_power2(*this, power))
    return rhs << mkUInt(power, sort());
  return binopc(Z3_mk_bvmul, operator*, Z3_OP_BMUL, isOne, isZero);
}

expr expr::sdiv(const expr &rhs) const {
  if (eq(rhs))
    return mkUInt(1, sort());

  if (rhs.isZero())
    return rhs;

  if (isZero())
    return *this;

  if (isSMin() && rhs.isAllOnes())
    return mkUInt(0, sort());

  return binop_fold(rhs, Z3_mk_bvsdiv);
}

expr expr::udiv(const expr &rhs) const {
  if (eq(rhs))
    return mkUInt(1, sort());

  if (rhs.isZero())
    return rhs;

  if (isZero())
    return *this;

  return binop_fold(rhs, Z3_mk_bvudiv);
}

expr expr::srem(const expr &rhs) const {
  if (eq(rhs) || isZero() || (isSMin() && rhs.isAllOnes()))
    return mkUInt(0, sort());

  if (rhs.isZero())
    return rhs;

  if (isNegative().isFalse() && rhs.isNegative().isFalse())
    return urem(rhs);

  return binop_fold(rhs, Z3_mk_bvsrem);
}

expr expr::urem(const expr &rhs) const {
  C();
  if (eq(rhs) || isZero())
    return mkUInt(0, sort());

  uint64_t n, log;
  if (rhs.isUInt(n)) {
    if (n <= 1)
      return mkUInt(0, sort());

    if (is_power2(n, &log))
      return mkUInt(0, bits() - log).concat(extract(log - 1, 0));
  }
  return binop_fold(rhs, Z3_mk_bvurem);
}

expr expr::sadd_sat(const expr &rhs) const {
  expr add_ext = sext(1) + rhs.sext(1);
  auto bw = bits();
  auto min = IntSMin(bw);
  auto max = IntSMax(bw);
  return mkIf(add_ext.sle(min.sext(1)),
              min,
              mkIf(add_ext.sge(max.sext(1)),
                   max,
                   *this + rhs));
}

expr expr::uadd_sat(const expr &rhs) const {
  return mkIf(add_no_uoverflow(rhs),
              *this + rhs,
              IntUMax(bits()));
}

expr expr::ssub_sat(const expr &rhs) const {
  expr sub_ext = sext(1) - rhs.sext(1);
  auto bw = bits();
  auto min = IntSMin(bw);
  auto max = IntSMax(bw);
  return mkIf(sub_ext.sle(min.sext(1)),
              min,
              mkIf(sub_ext.sge(max.sext(1)),
                   max,
                   *this - rhs));
}

expr expr::usub_sat(const expr &rhs) const {
  C();
  return mkIf(rhs.uge(*this),
              mkUInt(0, sort()),
              *this - rhs);
}

expr expr::sshl_sat(const expr &rhs) const {
  C();
  return mkIf(shl_no_soverflow(rhs), *this << rhs,
              mkIf(isNegative(), IntSMin(bits()), IntSMax(bits())));
}

expr expr::ushl_sat(const expr &rhs) const {
  C();
  return mkIf(shl_no_uoverflow(rhs), *this << rhs, IntUMax(bits()));
}

expr expr::add_no_soverflow(const expr &rhs) const {
  if (min_leading_zeros() >= 2 && rhs.min_leading_zeros() >= 2)
    return true;

  if (rhs.isConst()) {
    auto v = IntSMin(bits()) - rhs;
    return rhs.isNegative().isTrue() ? sge(v)
                                     : sle(v - mkUInt(1, rhs.sort()));
  }
  if (isConst())
    return rhs.add_no_soverflow(*this);

  return sext(1) + rhs.sext(1) == (*this + rhs).sext(1);
}

expr expr::add_no_uoverflow(const expr &rhs) const {
  if (min_leading_zeros() >= 1 && rhs.min_leading_zeros() >= 1)
    return true;

  if (rhs.isConst())
    return ule(mkInt(-1, rhs.sort()) - rhs);
  if (isConst())
    return rhs.add_no_uoverflow(*this);

  return (zext(1) + rhs.zext(1)).sign() == 0;
}

expr expr::add_no_usoverflow(const expr &rhs) const {
  if (min_leading_zeros() >= 1 && rhs.min_leading_zeros() >= 1)
    return true;

  return (zext(1) + rhs.sext(1)).sign() == 0;
}

expr expr::sub_no_soverflow(const expr &rhs) const {
  if (min_leading_zeros() >= 1 && rhs.min_leading_zeros() >= 1)
    return true;
  return sext(1) - rhs.sext(1) == (*this - rhs).sext(1);
}

expr expr::sub_no_uoverflow(const expr &rhs) const {
  return (zext(1) - rhs.zext(1)).sign() == 0;
}

expr expr::mul_no_soverflow(const expr &rhs) const {
  auto bw = bits();
  return sext(bw) * rhs.sext(bw) == (*this * rhs).sext(bw);
}

expr expr::mul_no_uoverflow(const expr &rhs) const {
  auto bw = bits();
  return (zext(bw) * rhs.zext(bw)).extract(2*bw - 1, bw) == 0;
}

expr expr::sdiv_exact(const expr &rhs) const {
  return sdiv(rhs) * rhs == *this;
}

expr expr::udiv_exact(const expr &rhs) const {
  return udiv(rhs) * rhs == *this;
}

expr expr::operator<<(const expr &rhs) const {
  C();
  if (isZero() || rhs.isZero())
    return *this;

  uint64_t shift;
  if (rhs.isUInt(shift)) {
    auto bw = bits();
    if (shift >= bw)
      return mkUInt(0, sort());
    return extract(bw-shift-1, 0).concat(mkUInt(0, shift));
  }

  return binop_fold(rhs, Z3_mk_bvshl);
}

expr expr::ashr(const expr &rhs) const {
  if (isZero() || rhs.isZero())
    return *this;

  return binop_fold(rhs, Z3_mk_bvashr);
}

expr expr::lshr(const expr &rhs) const {
  C();
  if (isZero() || rhs.isZero())
    return *this;

  uint64_t shift;
  if (rhs.isUInt(shift)) {
    auto bw = bits();
    if (shift >= bw)
      return mkUInt(0, sort());
    return mkUInt(0, shift).concat(extract(bw-1, shift));
  }

  return binop_fold(rhs, Z3_mk_bvlshr);
}

expr expr::fshl(const expr &a, const expr &b, const expr &c) {
  C2(a);
  auto width = mkUInt(a.bits(), a.sort());
  expr c_mod_width = c.urem(width);
  return a << c_mod_width | b.lshr(width - c_mod_width);
}

expr expr::fshr(const expr &a, const expr &b, const expr &c) {
  C2(a);
  auto width = mkUInt(a.bits(), a.sort());
  expr c_mod_width = c.urem(width);
  return a << (width - c_mod_width) | b.lshr(c_mod_width);
}

/*
 * FIXME: the fixed point functions are allowed to round towards zero
 * or towards negative, but this (and its unsigned friend) only
 * implements the round-towards-zero behavior. we'll need to fix this
 * if we run across a target rounding the other way.
 */
static expr smul_fix_helper(const expr &a, const expr &b, const expr &c) {
  auto width = a.bits();
  expr a2 = a.sext(width), b2 = b.sext(width);
  expr mul = a2 * b2;
  expr scale2 = c.zextOrTrunc(2 * width);
  return mul.ashr(scale2);
}

expr expr::smul_fix(const expr &a, const expr &b, const expr &c) {
  C2(a);
  expr r = smul_fix_helper(a, b, c);
  return r.trunc(a.bits());
}

expr expr::smul_fix_no_soverflow(const expr &a, const expr &b, const expr &c) {
  C2(a);
  expr r = smul_fix_helper(a, b, c);
  auto width = a.bits();
  expr result = r.trunc(width);
  return result.sext(width) == r;
}

expr expr::smul_fix_sat(const expr &a, const expr &b, const expr &c) {
  C2(a);
  expr r = smul_fix_helper(a, b, c);
  auto width = a.bits();
  return mkIf(smul_fix_no_soverflow(a, b, c),
              smul_fix(a, b, c),
              mkIf(r.isNegative(), IntSMin(width), IntSMax(width)));
}

static expr umul_fix_helper(const expr &a, const expr &b, const expr &c) {
  auto width = a.bits();
  expr a2 = a.zext(width), b2 = b.zext(width);
  expr mul = a2 * b2;
  expr scale2 = c.zextOrTrunc(2 * width);
  return mul.lshr(scale2);
}

expr expr::umul_fix(const expr &a, const expr &b, const expr &c) {
  C2(a);
  expr r = umul_fix_helper(a, b, c);
  return r.trunc(a.bits());
}

expr expr::umul_fix_no_uoverflow(const expr &a, const expr &b, const expr &c) {
  C2(a);
  auto width = a.bits();
  return umul_fix_helper(a, b, c).extract(width * 2 - 1, width) == 0;
}

expr expr::umul_fix_sat(const expr &a, const expr &b, const expr &c) {
  auto width = a.bits();
  return mkIf(umul_fix_no_uoverflow(a, b, c),
              umul_fix(a, b, c),
              IntUMax(width));
}

expr expr::shl_no_soverflow(const expr &rhs) const {
  return (*this << rhs).ashr(rhs) == *this;
}

expr expr::shl_no_uoverflow(const expr &rhs) const {
  return (*this << rhs).lshr(rhs) == *this;
}

expr expr::ashr_exact(const expr &rhs) const {
  return (ashr(rhs) << rhs) == *this;
}

expr expr::lshr_exact(const expr &rhs) const {
  return (lshr(rhs) << rhs) == *this;
}

static expr log2_rec(const expr &e, unsigned idx, unsigned bw) {
  if (idx == 0)
    return expr::mkUInt(0, bw);

  return expr::mkIf(e.extract(idx, idx) == 1,
                    expr::mkUInt(idx, bw),
                    log2_rec(e, idx - 1, bw));
}

expr expr::isPowerOf2() const {
  return *this != 0 && (*this & (*this - mkUInt(1, *this))) == 0;
}

expr expr::log2(unsigned bw_output) const {
  C();
  return log2_rec(*this, bits() - 1, bw_output);
}

expr expr::bswap() const {
  C();
  auto nbits = bits();
  constexpr unsigned bytelen = 8;

  assert(nbits % (bytelen * 2) == 0);
  expr res = extract(bytelen - 1, 0);
  for (unsigned i = 1; i < nbits / bytelen; i++) {
    res = res.concat(extract((i + 1) * bytelen - 1, i * bytelen));
  }

  return res;
}

expr expr::bitreverse() const {
  C();
  auto nbits = bits();

  expr res = extract(0, 0);
  for (unsigned i = 1; i < nbits; ++i) {
    res = res.concat(extract(i, i));
  }

  return res;
}

expr expr::cttz(const expr &val_zero) const {
  C();
  auto srt = sort();
  auto cond = val_zero;
  for (int i = bits() - 1; i >= 0; --i) {
    cond = mkIf(extract(i, i) == 1u, mkUInt(i, srt), cond);
  }

  return cond;
}

expr expr::ctlz() const {
  C();
  auto nbits = bits();
  auto srt = sort();

  auto cond = mkUInt(nbits, srt);
  for (unsigned i = 0; i < nbits; ++i) {
    cond = mkIf(extract(i, i) == 1u, mkUInt(nbits - 1 - i, srt), cond);
  }

  return cond;
}

expr expr::ctpop() const {
  C();
  auto nbits = bits();

  auto res = mkUInt(0, sort());
  for (unsigned i = 0; i < nbits; ++i) {
    res = res + extract(i, i).zext(nbits - 1);
  }

  return res;
}

expr expr::umin(const expr &rhs) const {
  return mkIf(ule(rhs), *this, rhs);
}

expr expr::umax(const expr &rhs) const {
  return mkIf(uge(rhs), *this, rhs);
}

expr expr::smin(const expr &rhs) const {
  return mkIf(sle(rhs), *this, rhs);
}

expr expr::smax(const expr &rhs) const {
  return mkIf(sge(rhs), *this, rhs);
}

expr expr::abs() const {
  C();
  auto s = sort();
  return mkIf(sge(mkUInt(0, s)), *this, mkInt(-1, s) * *this);
}

expr expr::round_up(const expr &power_of_two) const {
  expr minus_1 = power_of_two - mkUInt(1, power_of_two);
  return (*this + minus_1) & ~minus_1;
}

#define fold_fp_neg(fn)                                  \
  do {                                                   \
  expr cond, neg, v, v2;                                 \
  if (isIf(cond, neg, v) && neg.isFPNeg(v2) && v.eq(v2)) \
    return v.fn();                                       \
} while (0)

expr expr::isNaN() const {
  fold_fp_neg(isNaN);

  expr v;
  if (isFPNeg(v) || isFAbs(v))
    return v.isNaN();

  return unop_fold(Z3_mk_fpa_is_nan);
}

expr expr::isInf() const {
  fold_fp_neg(isInf);

  expr v;
  if (isFPNeg(v))
    return v.isInf();

  return unop_fold(Z3_mk_fpa_is_infinite);
}

expr expr::isFPZero() const {
  if (isBV())
    return extract(bits()-2, 0) == 0;
  return unop_fold(Z3_mk_fpa_is_zero);
}

expr expr::isFPNegative() const {
  return unop_fold(Z3_mk_fpa_is_negative);
}

expr expr::isFPNegZero() const {
  return isFPZero() && isFPNegative();
}

expr expr::isFPNormal() const {
  return unop_fold(Z3_mk_fpa_is_normal);
}

expr expr::isFPSubNormal() const {
  return unop_fold(Z3_mk_fpa_is_subnormal);
}

expr expr::rne() {
  return Z3_mk_fpa_rne(ctx());
}

expr expr::rna() {
  return Z3_mk_fpa_rna(ctx());
}

expr expr::rtp() {
  return Z3_mk_fpa_rtp(ctx());
}

expr expr::rtn() {
  return Z3_mk_fpa_rtn(ctx());
}

expr expr::rtz() {
  return Z3_mk_fpa_rtz(ctx());
}

expr expr::fadd(const expr &rhs, const expr &rm) const {
  C(rhs, rm);
  return simplify_const(Z3_mk_fpa_add(ctx(), rm(), ast(), rhs()), *this, rhs);
}

expr expr::fsub(const expr &rhs, const expr &rm) const {
  C(rhs, rm);
  return simplify_const(Z3_mk_fpa_sub(ctx(), rm(), ast(), rhs()), *this, rhs);
}

expr expr::fmul(const expr &rhs, const expr &rm) const {
  C(rhs, rm);
  return simplify_const(Z3_mk_fpa_mul(ctx(), rm(), ast(), rhs()), *this, rhs);
}

expr expr::fdiv(const expr &rhs, const expr &rm) const {
  C(rhs, rm);
  return simplify_const(Z3_mk_fpa_div(ctx(), rm(), ast(), rhs()), *this, rhs);
}

expr expr::frem(const expr &rhs) const {
  C(rhs);
  return simplify_const(Z3_mk_fpa_rem(ctx(), ast(), rhs()), *this, rhs);
}

expr expr::fabs() const {
  if (isBV())
    return mkUInt(0, 1).concat(extract(bits() - 2, 0));

  fold_fp_neg(fabs);
  return unop_fold(Z3_mk_fpa_abs);
}

expr expr::fneg() const {
  if (isBV()) {
    auto signbit = bits() - 1;
    return (extract(signbit, signbit) ^ mkUInt(1, 1))
             .concat(extract(signbit - 1, 0));
  }
  return unop_fold(Z3_mk_fpa_neg);
}

expr expr::copysign(const expr &sign) const {
  auto sign_bit = sign.bits() - 1;
  return sign.extract(sign_bit, sign_bit).concat(extract(bits() - 2, 0));
}

expr expr::sqrt(const expr &rm) const {
  C(rm);
  return simplify_const(Z3_mk_fpa_sqrt(ctx(), rm(), ast()), *this);
}

expr expr::fma(const expr &a, const expr &b, const expr &c, const expr &rm) {
  C2(a, b, c, rm);
  return simplify_const(Z3_mk_fpa_fma(ctx(), rm(), a(), b(), c()), a, b, c);
}

expr expr::ceil() const {
  return round(rtp());
}

expr expr::floor() const {
  return round(rtn());
}

expr expr::round(const expr &rm) const {
  C(rm);
  return
    simplify_const(Z3_mk_fpa_round_to_integral(ctx(), rm(), ast()), *this);
}

expr expr::foeq(const expr &rhs) const {
  return binop_commutative(rhs, Z3_mk_fpa_eq);
}

expr expr::fogt(const expr &rhs) const {
  return binop_fold(rhs, Z3_mk_fpa_gt);
}

expr expr::foge(const expr &rhs) const {
  return binop_fold(rhs, Z3_mk_fpa_geq);
}

expr expr::folt(const expr &rhs) const {
  return binop_fold(rhs, Z3_mk_fpa_lt);
}

expr expr::fole(const expr &rhs) const {
  return binop_fold(rhs, Z3_mk_fpa_leq);
}

expr expr::fone(const expr &rhs) const {
  return ford(rhs) && !binop_commutative(rhs, Z3_mk_fpa_eq);
}

expr expr::ford(const expr &rhs) const {
  return !isNaN() && !rhs.isNaN();
}

expr expr::fueq(const expr &rhs) const {
  return funo(rhs) || binop_commutative(rhs, Z3_mk_fpa_eq);
}

expr expr::fugt(const expr &rhs) const {
  return funo(rhs) || binop_fold(rhs, Z3_mk_fpa_gt);
}

expr expr::fuge(const expr &rhs) const {
  return funo(rhs) || binop_fold(rhs, Z3_mk_fpa_geq);
}

expr expr::fult(const expr &rhs) const {
  return funo(rhs) || binop_fold(rhs, Z3_mk_fpa_lt);
}

expr expr::fule(const expr &rhs) const {
  return funo(rhs) || binop_fold(rhs, Z3_mk_fpa_leq);
}

expr expr::fune(const expr &rhs) const {
  return funo(rhs) || !binop_commutative(rhs, Z3_mk_fpa_eq);
}

expr expr::funo(const expr &rhs) const {
  return isNaN() || rhs.isNaN();
}

static expr get_bool(const expr &e) {
  expr cond, then, els;
  if (e.isIf(cond, then, els)) {
    if (then.isOne() && els.isZero())
      return cond;
    if (then.isZero() && els.isOne())
      return !cond;
  }
  return {};
}

expr expr::operator&(const expr &rhs) const {
  if (eq(rhs) || isZero() || rhs.isAllOnes())
    return *this;
  if (isAllOnes() || rhs.isZero())
    return rhs;

  auto fold_extract = [](auto &a, auto &b) {
    uint64_t n;
    if (!a.isUInt(n) || n == 0 || n == numeric_limits<uint64_t>::max())
      return expr();

    auto lead  = countl_zero(n);
    auto trail = countr_zero(n);

    if (!is_power2((n >> trail) + 1))
      return expr();

    auto r = b.extract(63 - lead, trail);
    lead -= 64 - a.bits();
    if (lead > 0)
      r = mkUInt(0, lead).concat(r);
    if (trail > 0)
      r = r.concat(mkUInt(0, trail));
    return r;
  };

  if (bits() <= 64) {
    if (auto f = fold_extract(*this, rhs);
        f.isValid())
      return f;
    if (auto f = fold_extract(rhs, *this);
        f.isValid())
      return f;

    if (bits() == 1) {
      if (auto a = get_bool(*this);
          a.isValid())
        if(auto b = get_bool(rhs);
           b.isValid())
          return (a && b).toBVBool();
    }
  }
  return binopc(Z3_mk_bvand, operator&, Z3_OP_BAND, isAllOnes, isZero);
}

expr expr::operator|(const expr &rhs) const {
  if (eq(rhs) || isAllOnes() || rhs.isZero())
    return *this;
  if (isZero() || rhs.isAllOnes())
    return rhs;

  if (bits() == 1) {
    if (auto a = get_bool(*this);
        a.isValid())
      if (auto b = get_bool(rhs);
        b.isValid())
        return (a || b).toBVBool();
  }

  return binopc(Z3_mk_bvor, operator|, Z3_OP_BOR, isZero, isAllOnes);
}

expr expr::operator^(const expr &rhs) const {
  if (eq(rhs))
    return mkUInt(0, sort());
  if (isAllOnes())
    return bits() == 1 ? (rhs == 0).toBVBool() : ~rhs;
  if (rhs.isAllOnes())
    return bits() == 1 ? (*this == 0).toBVBool() : ~*this;
  return binopc(Z3_mk_bvxor, operator^, Z3_OP_BXOR, isZero, alwaysFalse);
}

expr expr::operator!() const {
  C();
  if (isTrue())
    return false;
  if (isFalse())
    return true;

  expr e;
  if (isNot(e))
    return e;
  return Z3_mk_not(ctx(), ast());
}

expr expr::operator~() const {
  return unop_fold(Z3_mk_bvnot);
}

expr expr::cmp_eq(const expr &rhs, bool simplify) const {
  if (!simplify)
    goto end;

  if (eq(rhs))
    return true;
  if (isConst()) {
    if (rhs.isConst())
      return false;
    return rhs == *this;
  }
  // constants on rhs from now.

  if (rhs.isTrue())
    return *this;
  if (rhs.isFalse())
    return !*this;

  // (= (= a bit) (= b bit)) -> (= a b)
  {
    expr lhs_a, lhs_b, rhs_a, rhs_b;
    if (isEq(lhs_a, lhs_b) && lhs_a.isBV() && lhs_a.bits() == 1 &&
        rhs.isEq(rhs_a, rhs_b) && lhs_a.eq(rhs_a))
      return lhs_b == rhs_b;
  }

  // (= (+ a c1) (+ a c2)) -> false
  {
    expr lhs_base, rhs_base;
    uint64_t lhs_offset, rhs_offset;
    if (isBasePlusOffset(lhs_base, lhs_offset) &&
        rhs.isBasePlusOffset(rhs_base, rhs_offset) &&
        lhs_base.eq(rhs_base) &&
        lhs_offset != rhs_offset)
      return false;
  }

  if (auto app = isAppOf(Z3_OP_CONCAT)) {
    unsigned num_args = Z3_get_app_num_args(ctx(), app);
    // (concat x y) == const -> x == const[..] /\ y == const[..]
    if (rhs.isConst()) {
      AndExpr eqs;
      unsigned high = bits();
      for (unsigned i = 0; i < num_args; ++i) {
        expr arg = Z3_get_app_arg(ctx(), app, i);
        unsigned low = high - arg.bits();
        eqs.add(arg == rhs.extract(high - 1, low));
        high = low;
      }
      return eqs();
    }

    // (concat ..) == (concat ..)
    if (auto app_rhs = rhs.isAppOf(Z3_OP_CONCAT);
        app_rhs != nullptr &&
        num_args == Z3_get_app_num_args(ctx(), app_rhs)) {
      AndExpr eqs;
      bool all_aligned = true;
      unsigned l_idx = 0, r_idx = 0;
      for (unsigned i = 0; i < num_args; ++i) {
        expr lhs = Z3_get_app_arg(ctx(), app, i);
        expr rhs = Z3_get_app_arg(ctx(), app_rhs, i);
        unsigned l_bits = lhs.bits();
        unsigned r_bits = rhs.bits();
        all_aligned &= l_bits == r_bits;

        if (l_idx == r_idx && l_bits == r_bits) {
          eqs.add(lhs == rhs);
        }
        // even if the concats aren't aligned, we still compare the constant
        // bits in an attempt to prove the comparison false
        else if (lhs.isConst() && rhs.isConst()) {
          // r .. l .. r+sz
          if (l_idx >= r_idx && l_idx < (r_idx + r_bits)) {
            unsigned overlap = min(r_bits - (l_idx - r_idx), l_bits);
            unsigned r_off = r_idx + r_bits - (l_idx + overlap);
            eqs.add(lhs.extract(l_bits-1, l_bits - overlap) ==
                    rhs.extract(r_off + overlap - 1, r_off));
          }
          else if (r_idx >= l_idx && r_idx < (l_idx + l_bits)) {
            unsigned overlap = min(l_bits - (r_idx - l_idx), r_bits);
            unsigned l_off = l_idx + l_bits - (r_idx + overlap);
            eqs.add(lhs.extract(l_off + overlap - 1, l_off) ==
                    rhs.extract(r_bits-1, r_bits - overlap));
          }
        }
        l_idx += l_bits;
        r_idx += r_bits;
      }
      assert(l_idx == r_idx);

      if (all_aligned || !eqs)
        return eqs();
    }
  }

  {
    expr c, t, e;
    if (isIf(c, t, e)) {
      // (= (ite c t e) (ite c2 t e)) -> c == c2
      expr c2, t2, e2;
      if (rhs.isIf(c2, t2, e2) && t.eq(t2) && e.eq(e2) && !t.eq(e) &&
          t.isConst() && e.isConst())
        return c == c2;

      // (= (ite c t e) x) -> (ite c (= t x) (= e x))
      if ((rhs.isConst() && (t.isConst() || e.isConst())) ||
          (t.isConst() && e.isConst() && !rhs.isVar()))
        return mkIf(c, t == rhs, e == rhs);
    }
    else if (rhs.isAppOf(Z3_OP_ITE)) {
      return rhs == *this;
    }
  }

  {
    expr a, b;
    if (isAdd(a, b)) {
      // Pre: a >= 0, b >= 0, rhs u< b
      // a + b == rhs -> false
      if (a.isNegative().isFalse() &&
          b.isNegative().isFalse() &&
          (rhs.ult(a).isTrue() || rhs.ult(b).isTrue()))
        return false;
    }

    // (a u% b) == c -> false if c u>= b
    if (isURem(a, b) && rhs.uge(b).isTrue())
      return false;

    if (isSignExt(a)) {
      // (sext a) == (sext b) -> a == b
      if (rhs.isSignExt(b) && a.bits() == b.bits())
        return a == b;

      // (sext a) == 0 -> a == 0
      if (rhs.isZero())
        return a == mkUInt(0, a);
    }
  }

end:
  return binop_commutative(rhs, Z3_mk_eq);
}

expr expr::operator==(const expr &rhs) const {
  return cmp_eq(rhs, true);
}

expr expr::operator!=(const expr &rhs) const {
  return !(*this == rhs);
}

expr expr::operator&&(const expr &rhs) const {
  if (eq(rhs) || isFalse() || rhs.isTrue())
    return *this;
  if (isTrue() || rhs.isFalse())
    return rhs;

  C(rhs);
  Z3_ast args[] = { ast(), rhs() };
  return Z3_mk_and(ctx(), 2, args);
}

expr expr::operator||(const expr &rhs) const {
  if (eq(rhs) || rhs.isFalse() || isTrue())
    return *this;
  if (rhs.isTrue() || isFalse())
    return rhs;

  expr n;
  if ((isNot(n) && n.eq(rhs)) ||
      (rhs.isNot(n) && eq(n)))
    return true;

  // (a & b) | (!a & b) -> b
  expr a, b, c, d;
  if (isAnd(a, b) && rhs.isAnd(c, d) && b.eq(d)) {
    if ((a.isNot(n) && n.eq(c)) ||
        (c.isNot(n) && n.eq(a)))
      return b;
  }

  C(rhs);
  Z3_ast args[] = { ast(), rhs() };
  return Z3_mk_or(ctx(), 2, args);
}

void expr::operator&=(const expr &rhs) {
  *this = *this && rhs;
}

void expr::operator|=(const expr &rhs) {
  *this = *this || rhs;
}

expr expr::mk_and(const set<expr> &vals) {
  expr ret(true);
  for (auto &e : vals) {
    ret &= e;
  }
  return ret;
}

expr expr::mk_or(const set<expr> &vals) {
  expr ret(false);
  for (auto &e : vals) {
    ret |= e;
  }
  return ret;
}

expr expr::implies(const expr &rhs) const {
  if (eq(rhs))
    return true;
  return !*this || rhs;
}

expr expr::notImplies(const expr &rhs) const {
  if (eq(rhs))
    return false;
  return *this && !rhs;
}

expr expr::ule(const expr &rhs) const {
  if (eq(rhs) || isZero() || rhs.isAllOnes())
    return true;
  if (rhs.isZero() || isAllOnes())
    return *this == rhs;

  // 00... <= ..111 -> true
  if (min_leading_zeros() + rhs.min_trailing_ones() >= bits())
    return true;

  return binop_fold(rhs, Z3_mk_bvule);
}

expr expr::ult(const expr &rhs) const {
  C();
  uint64_t n;
  if (rhs.isUInt(n))
    return n == 0 ? false : ule(mkUInt(n - 1, sort()));

  return !rhs.ule(*this);
}

expr expr::uge(const expr &rhs) const {
  return rhs.ule(*this);
}

expr expr::ugt(const expr &rhs) const {
  C();
  if (rhs.isAllOnes())
    return false;

  uint64_t n;
  if (rhs.isUInt(n)) {
    auto ty = sort();
    return uge(mkUInt(n, ty) + mkUInt(1, ty));
  }
  return !ule(rhs);
}

expr expr::sle(const expr &rhs) const {
  if (eq(rhs) || rhs.isSMax())
    return true;

  return binop_fold(rhs, Z3_mk_bvsle);
}

expr expr::slt(const expr &rhs) const {
  C();
  if (rhs.isSMin())
    return false;

  int64_t n;
  if (rhs.isInt(n))
    return sle(mkInt(n - 1, sort()));

  return !rhs.sle(*this);
}

expr expr::sge(const expr &rhs) const {
  return rhs.sle(*this);
}

expr expr::sgt(const expr &rhs) const {
  return !sle(rhs);
}

expr expr::ule(uint64_t rhs) const {
  C();
  return ule(mkUInt(rhs, sort()));
}

expr expr::ult(uint64_t rhs) const {
  C();
  return ult(mkUInt(rhs, sort()));
}

expr expr::uge(uint64_t rhs) const {
  C();
  return uge(mkUInt(rhs, sort()));
}

expr expr::ugt(uint64_t rhs) const {
  C();
  return ugt(mkUInt(rhs, sort()));
}

expr expr::sle(int64_t rhs) const {
  C();
  return sle(mkInt(rhs, sort()));
}

expr expr::sge(int64_t rhs) const {
  C();
  return sge(mkInt(rhs, sort()));
}

expr expr::operator==(uint64_t rhs) const {
  C();
  return *this == mkUInt(rhs, sort());
}

expr expr::operator!=(uint64_t rhs) const {
  C();
  return *this != mkUInt(rhs, sort());
}

expr expr::sext(unsigned amount) const {
  C();
  if (amount == 0)
    return *this;

  expr e;
  if (isSignExt(e))
    return e.sext((bits() - e.bits()) + amount);

  if (isNegative().isFalse())
    return zext(amount);

  return simplify_const(Z3_mk_sign_ext(ctx(), amount, ast()), *this);
}

expr expr::zext(unsigned amount) const {
  if (amount == 0)
    return *this;
  return mkUInt(0, amount).concat(*this);
}

expr expr::trunc(unsigned tobw) const {
  return extract(tobw-1, 0);
}

expr expr::sextOrTrunc(unsigned tobw) const {
  auto bw = bits();
  return bw < tobw ? sext(tobw - bw) : trunc(tobw);
}

expr expr::zextOrTrunc(unsigned tobw) const {
  auto bw = bits();
  return bw < tobw ? zext(tobw - bw) : trunc(tobw);
}

expr expr::concat(const expr &rhs) const {
  expr a, b, c, d;
  unsigned h, l, h2, l2;
  if (isExtract(a, h, l)) {
    if (rhs.isExtract(b, h2, l2) && l == h2+1) {
      if (a.eq(b))
        return a.extract(h, l2);

      // (concat (extract (concat const X)) (extract X))
      expr aa, ab;
      if (l2 == 0 && a.isConcat(aa, ab) && ab.eq(b) &&
          (h - aa.bits()) == b.bits())
        return aa.concat(b);
    }

    //  extract_l concat (concat extract_r foo)
    if (rhs.isConcat(b, c) && b.isExtract(d, h2, l2) && l == h2+1 && a.eq(d))
      return a.extract(h, l2).concat(c);
  }

  // (concat (concat x extract) extract)
  if (isConcat(a, b) && b.isExtract(c, h, l) && rhs.isExtract(d, h2, l2) &&
      l == h2+1 && c.eq(d))
    return a.concat(c.extract(h, l2));

  // (concat const (concat const2 x))
  if (isConst() && rhs.isConcat(a, b) && a.isConst())
    return concat(a).concat(b);

  return binop_fold(rhs, Z3_mk_concat);
}

expr expr::concat_zeros(unsigned bits) const {
  return bits ? concat(mkUInt(0, bits)) : *this;
}

expr expr::extract(unsigned high, unsigned low, unsigned depth) const {
  C();
  assert(high >= low && high < bits());

  if (low == 0 && high == bits()-1)
    return *this;

  if (depth-- == 0)
    goto end;

  {
    expr sub;
    unsigned high_2, low_2;
    if (isExtract(sub, high_2, low_2))
      return sub.extract(high + low_2, low + low_2);
  }
  if (low == 0) {
    expr val;
    if (isSignExt(val)) {
      auto val_bits = val.bits();
      if (high < val_bits)
        return val.extract(high, 0);
      return val.sext(high - val_bits + 1);
    }
  }
  {
    expr a, b;
    if (isConcat(a, b)) {
      auto b_bw = b.bits();
      if (high < b_bw)
        return b.extract(high, low);
      if (low >= b_bw)
        return a.extract(high - b_bw, low - b_bw);
      if (low == 0)
        return a.extract(high - b_bw, 0).concat(b);
      if (a.isConst() || b.isConst())
        return a.extract(high - b_bw, 0).concat(b.extract(b_bw-1, low));
    }
  }
  {
    expr cond, then, els;
    if (isIf(cond, then, els)) {
      then = then.extract(high, low, depth);
      els = els.extract(high, low, depth);
      if (then.eq(els))
        return then;
      if (then.isConst() && els.isConst())
        return mkIf(cond, then, els);
    }
  }
  {
    auto simpl_bitwise = [&](int type, bool (expr::*absorvent)() const) {
      if (auto app = isAppOf(type)) {
        expr extracted;
        bool first = true;
        unsigned num_args = Z3_get_app_num_args(ctx(), app);

        for (unsigned i = 0; i < num_args; ++i) {
          expr arg
            = expr(Z3_get_app_arg(ctx(), app, i)).extract(high, low, depth);

          // extract (op a b absorvent) -> absorvent
          if ((arg.*absorvent)())
            return arg;

          if (first)
            extracted = std::move(arg);
          else if (!arg.eq(extracted))
            extracted = expr();
          first = false;
        }

        // extract (op a b) -> (op (extract a) (extract b)) -> (extract a)
        // if (extract a) = (extract b)
        if (extracted.isValid())
          return extracted;
      }
      return expr();
    };
    if (auto ret = simpl_bitwise(Z3_OP_BAND, &expr::isZero); ret.isValid())
      return ret;
    if (auto ret = simpl_bitwise(Z3_OP_BOR, &expr::isAllOnes); ret.isValid())
      return ret;
  }
end:
  return simplify_const(Z3_mk_extract(ctx(), high, low, ast()), *this);
}

expr expr::sign() const {
  auto bit = bits() - 1;
  return extract(bit, bit);
}

expr expr::toBVBool() const {
  auto sort = mkBVSort(1);
  return mkIf(*this, mkUInt(1, sort), mkUInt(0, sort));
}

expr expr::float2BV() const {
  if (auto app = isAppOf(Z3_OP_FPA_TO_FP)) // ((_ to_fp e s) BV)
    if (Z3_get_app_num_args(ctx(), app) == 1)
      return Z3_get_app_arg(ctx(), app, 0);

  return unop_fold(Z3_mk_fpa_to_ieee_bv);
}

expr expr::float2Real() const {
  return unop_fold(Z3_mk_fpa_to_real);
}

expr expr::BV2float(const expr &type) const {
  C(type);
  if (auto app = isAppOf(Z3_OP_FPA_TO_IEEE_BV)) {
    expr arg = Z3_get_app_arg(ctx(), app, 0);
    if (arg.sort() == type.sort())
      return arg;
  }

  return simplify_const(Z3_mk_fpa_to_fp_bv(ctx(), ast(), type.sort()), *this);
}

expr expr::float2Float(const expr &type, const expr &rm) const {
  C(type, rm);
  return simplify_const(Z3_mk_fpa_to_fp_float(ctx(), rm(), ast(), type.sort()),
                        *this);
}

expr expr::fp2sint(unsigned bits, const expr &rm) const {
  C(rm);
  return simplify_const(Z3_mk_fpa_to_sbv(ctx(), rm(), ast(), bits), *this);
}

expr expr::fp2uint(unsigned bits, const expr &rm) const {
  C(rm);
  return simplify_const(Z3_mk_fpa_to_ubv(ctx(), rm(), ast(), bits), *this);
}

expr expr::sint2fp(const expr &type, const expr &rm) const {
  C(type, rm);
  return simplify_const(Z3_mk_fpa_to_fp_signed(ctx(), rm(), ast(), type.sort()),
                        *this);
}

expr expr::uint2fp(const expr &type, const expr &rm) const {
  C(type, rm);
  return
    simplify_const(Z3_mk_fpa_to_fp_unsigned(ctx(), rm(), ast(), type.sort()),
                   *this);
}

expr expr::mkUF(const char *name, const vector<expr> &args, const expr &range) {
  C2(range);
  auto num_args = args.size();
  vector<Z3_ast> z3_args;
  vector<Z3_sort> z3_sorts;
  z3_args.reserve(num_args);
  z3_sorts.reserve(num_args);

  for (auto &arg : args) {
    C2(arg);
    z3_args.emplace_back(arg());
    z3_sorts.emplace_back(arg.sort());
  }

  auto decl = Z3_mk_func_decl(ctx(), Z3_mk_string_symbol(ctx(), name),
                              num_args, z3_sorts.data(), range.sort());
  return Z3_mk_app(ctx(), decl, num_args, z3_args.data());
}

expr expr::mkArray(const char *name, const expr &domain, const expr &range) {
  C2(domain, range);
  return ::mkVar(name, Z3_mk_array_sort(ctx(), domain.sort(), range.sort()));
}

expr expr::mkConstArray(const expr &domain, const expr &value) {
  C2(domain, value);
  return Z3_mk_const_array(ctx(), domain.sort(), value());
}

expr expr::store(const expr &idx, const expr &val) const {
  C(idx, val);
  expr array, str_idx, str_val;
  if (isStore(array, str_idx, str_val)) {
    if ((idx == str_idx).isTrue())
      return array.store(idx, val);

  } else if (isConstArray(str_val)) {
    if (str_val.eq(val))
      return *this;
  }
  return Z3_mk_store(ctx(), ast(), idx(), val());
}

expr expr::load(const expr &idx, uint64_t max_idx) const {
  C(idx);

  // TODO: add support for alias analysis plugin
  expr array, str_idx, val;
  if (isStore(array, str_idx, val)) { // store(array, idx, val)
    uint64_t str_idx_val;
    // loaded idx can't possibly match the stored idx; there's UB otherwise
    if (str_idx.isUInt(str_idx_val) && str_idx_val > max_idx)
      return array.load(idx, max_idx);

    expr cmp = idx == str_idx;
    if (cmp.isTrue())
      return val;

    auto loaded = array.load(idx, max_idx);
    if (cmp.isFalse() || val.eq(loaded))
      return loaded;

  } else if (isConstArray(val)) {
    return val;
  } else if (isLambda(val)) {
    return val.subst({ idx }).foldTopLevel();
  }

  return Z3_mk_select(ctx(), ast(), idx());
}

expr expr::mkIf(const expr &cond, const expr &then, const expr &els) {
  C2(cond, then, els);
  if (cond.isTrue() || then.eq(els))
    return then;
  if (cond.isFalse())
    return els;

  if (then.isTrue() || cond.eq(then))
    return cond || els;
  if (then.isFalse())
    return !cond && els;
  if (els.isTrue())
    return !cond || then;
  if (els.isFalse())
    return cond && then;

  expr notcond;
  if (cond.isNot(notcond))
    return mkIf(notcond, els, then);

  expr lhs, rhs;
  // (ite (= x 1) 1 0) -> x
  // (ite (= x 1) 0 1) -> (not x)
  if (then.isBV() && then.bits() == 1 && cond.isEq(lhs, rhs) &&
      lhs.isBV() && lhs.bits() == 1) {
    if (then.isOne() && els.isZero()) {
      if (lhs.isOne())
        return rhs;
      if (rhs.isOne())
        return lhs;
    }
    if (then.isZero() && els.isOne()) {
      if (lhs.isOne())
        return ~rhs;
      if (rhs.isOne())
        return ~lhs;
    }
  }

  // (ite c a (ite c2 a b)) -> (ite (or c c2) a b)
  expr cond2, then2, else2;
  if (els.isIf(cond2, then2, else2) && then.eq(then2))
    return mkIf(cond || cond2, then, else2);

  return Z3_mk_ite(ctx(), cond(), then(), els());
}

expr expr::mkForAll(const set<expr> &vars, expr &&val) {
  if (vars.empty() || val.isConst() || !val.isValid())
    return std::move(val);

  auto vars_ast = make_unique<Z3_app[]>(vars.size());
  unsigned i = 0;
  for (auto &v : vars) {
    vars_ast[i++] = (Z3_app)v();
  }
  return Z3_mk_forall_const(ctx(), 0, vars.size(), vars_ast.get(), 0, nullptr,
                            val());
}

expr expr::mkLambda(const expr &var, const expr &val) {
  C2(var, val);

  if (!val.vars().count(var))
    return mkConstArray(var, val);

  expr array, idx;
  if (val.isLoad(array, idx) && idx.eq(var))
    return array;

  auto ast = (Z3_app)var();
  return Z3_mk_lambda_const(ctx(), 1, &ast, val());
}

expr expr::simplify() const {
  C();
  auto e = Z3_simplify(ctx(), ast());
  // Z3_simplify returns null on timeout
  return e ? e : *this;
}

expr expr::simplifyNoTimeout() const {
  C();
  return Z3_simplify_ex(ctx(), ast(), ctx.getNoTimeoutParam());
}

expr expr::foldTopLevel() const {
  expr cond, then, els;
  if (isIf(cond, then, els))
    return
      expr::mkIf(cond.foldTopLevel(), then.foldTopLevel(), els.foldTopLevel());

  expr array, idx;
  if (isLoad(array, idx) && idx.isConst())
    return array.load(idx);

  if (isApp()) {
    bool is_const = true;
    for (unsigned i = 0, e = getFnNumArgs(); i < e; ++i) {
      if (!(is_const &= getFnArg(i).isConst()))
        break;
    }
    if (is_const)
      return simplifyNoTimeout();
  }
  return *this;
}

expr expr::subst(const vector<pair<expr, expr>> &repls) const {
  C();
  if (repls.empty())
    return *this;

  auto from = make_unique<Z3_ast[]>(repls.size());
  auto to   = make_unique<Z3_ast[]>(repls.size());

  unsigned i = 0;
  for (auto &p : repls) {
    C2(p.first, p.second);
    from[i] = p.first();
    to[i] = p.second();
    ++i;
  }
  return Z3_substitute(ctx(), ast(), repls.size(), from.get(), to.get());
}

expr expr::subst(const expr &from, const expr &to) const {
  C(from, to);
  auto f = from();
  auto t = to();
  return Z3_substitute(ctx(), ast(), 1, &f, &t);
}

expr expr::subst(const vector<expr> &repls) const {
  C();
  if (repls.empty())
    return *this;

  unique_ptr<Z3_ast[]> vars(new Z3_ast[repls.size()]);
  unsigned i = 0;
  for (auto &v : repls) {
    C2(v);
    vars[i++] = v();
  }
  return Z3_substitute_vars(ctx(), ast(), repls.size(), vars.get());
}

set<expr> expr::vars() const {
  return vars({ this });
}

set<expr> expr::vars(const vector<const expr*> &exprs) {
  set<expr> result;
  vector<Z3_ast> todo;
  unordered_set<Z3_ast> seen;

  for (auto e : exprs) {
    C2(*e);
    auto ast = e->ast();
    if (seen.emplace(ast).second)
      todo.emplace_back(ast);
  }

  do {
    auto ast = todo.back();
    todo.pop_back();

    switch (Z3_get_ast_kind(ctx(), ast)) {
    case Z3_VAR_AST:
    case Z3_NUMERAL_AST:
      break;

    case Z3_QUANTIFIER_AST: {
      auto body = Z3_get_quantifier_body(ctx(), ast);
      if (seen.emplace(body).second)
        todo.emplace_back(body);
      break;
    }

    case Z3_APP_AST: {
      // Z3_NUMERAL_AST above only catches real numbers
      if (Z3_is_numeral_ast(ctx(), ast))
        continue;

      auto app = Z3_to_app(ctx(), ast);
      auto num_args = Z3_get_app_num_args(ctx(), app);
      if (num_args == 0) { // it's a variable
        result.emplace(expr(ast));
        continue;
      }

      for (unsigned i = 0; i < num_args; ++i) {
        auto arg = Z3_get_app_arg(ctx(), app, i);
        if (seen.emplace(arg).second)
          todo.emplace_back(arg);
      }
      break;
    }
    default:
      UNREACHABLE();
    }
  } while (!todo.empty());

  return result;
}

set<expr> expr::leafs(unsigned max) const {
  C();
  vector<expr> worklist = { *this };
  unordered_set<Z3_ast> seen;
  set<expr> ret;
  do {
    auto val = std::move(worklist.back());
    worklist.pop_back();
    if (!seen.emplace(val()).second)
      continue;

    expr cond, then, els, e;
    unsigned high, low;
    if (val.isIf(cond, then, els)) {
      worklist.emplace_back(std::move(then));
      worklist.emplace_back(std::move(els));
    } else if (val.isExtract(e, high, low) && e.isIf(cond, then, els)) {
      worklist.emplace_back(then.extract(high, low));
      worklist.emplace_back(els.extract(high, low));
    } else {
      ret.emplace(std::move(val));
    }

    if (ret.size() + worklist.size() >= max) {
      for (auto &v : worklist)
        ret.emplace(std::move(v));
      break;
    }
  } while (!worklist.empty());

  return ret;
}

set<expr> expr::get_apps_of(const char *fn_name, const char *prefix) const {
  C();
  vector<expr> worklist = { *this };
  unordered_set<Z3_ast> seen;
  set<expr> ret;
  do {
    auto val = std::move(worklist.back());
    worklist.pop_back();

    for (unsigned i = 0, e = val.getFnNumArgs(); i < e; ++i) {
      expr arg = val.getFnArg(i);
      if (arg.isApp() && seen.emplace(arg()).second)
        worklist.emplace_back(std::move(arg));
    }

    auto str = val.fn_name();
    if (str == fn_name || str.starts_with(prefix)) {
      ret.emplace(std::move(val));
    }
  } while (!worklist.empty());

  return ret;
}

void expr::printUnsigned(ostream &os) const {
  os << numeral_string();
}

void expr::printSigned(ostream &os) const {
  if (isNegative().isTrue()) {
    os << '-';
    (~*this + mkUInt(1, sort())).printUnsigned(os);
  } else {
    printUnsigned(os);
  }
}

void expr::printHexadecimal(ostream &os) const {
  auto rem = bits() % 4;
  os << (rem == 0 ? *this : zext(4 - rem));
}

string expr::numeral_string() const {
  C();
  return Z3_get_numeral_decimal_string(ctx(), ast(), 12);
}

string expr::fn_name() const {
  if (isApp())
    return Z3_get_symbol_string(ctx(), Z3_get_decl_name(ctx(), decl()));
  return {};
}

unsigned expr::getFnNumArgs() const {
  auto app = isApp();
  assert(app);
  return Z3_get_app_num_args(ctx(), app);
}

expr expr::getFnArg(unsigned i) const {
  auto app = isApp();
  assert(app && i < Z3_get_app_num_args(ctx(), app));
  return Z3_get_app_arg(ctx(), app, i);
}

ostream& operator<<(ostream &os, const expr &e) {
  return os << (e.isValid() ? Z3_ast_to_string(ctx(), e()) : "(null)");
}

strong_ordering expr::operator<=>(const expr &rhs) const {
  if (ptr == rhs.ptr || !isValid() || !rhs.isValid())
    return ptr <=> rhs.ptr;
  // so iterators are stable
  return id() <=> rhs.id();
}

unsigned expr::id() const {
  return Z3_get_ast_id(ctx(), ast());
}

unsigned expr::hash() const {
  return Z3_get_ast_hash(ctx(), ast());
}

}
