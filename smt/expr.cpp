// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/expr.h"
#include "smt/ctx.h"
#include "util/compiler.h"
#include <algorithm>
#include <cassert>
#include <limits>
#include <memory>
#include <string>
#include <unordered_set>
#include <z3.h>

#define DEBUG_Z3_RC 0

#if DEBUG_Z3_RC
# include <iostream>
#endif

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


namespace smt {

expr::expr(Z3_ast ast) : ptr((uintptr_t)ast) {
  static_assert(sizeof(Z3_ast) == sizeof(uintptr_t));
  assert(isZ3Ast() && isValid());
  incRef();
#if DEBUG_Z3_RC
  cout << "[Z3RC] newObj " << ast << ' ' << *this << '\n';
#endif
}

bool expr::isZ3Ast() const {
  return (ptr & 1) == 0;
}

Z3_ast expr::ast() const {
  assert(isValid());

  if (isZ3Ast())
    return (Z3_ast)ptr;

  assert(0 && "TODO");
  return 0;
}

expr::expr(const expr &other) {
  if (!other.isValid()) {
    ptr = 0;
  } else if (other.isZ3Ast()) {
    ptr = other.ptr;
    incRef();
  } else {
    assert(0 && "TODO");
  }
}

expr::~expr() {
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
  assert(app);
  return Z3_get_app_decl(ctx(), app);
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

expr expr::mkFloat(float n) {
  return Z3_mk_fpa_numeral_float(ctx(), n, Z3_mk_fpa_sort_single(ctx()));
}

expr expr::mkDouble(double n) {
  return Z3_mk_fpa_numeral_double(ctx(), n, Z3_mk_fpa_sort_double(ctx()));
}

expr expr::mkNumber(const char *n, const expr &type) {
  C2(type);
  return Z3_mk_numeral(ctx(), n, type.sort());
}

expr expr::mkConst(Z3_func_decl decl) {
  return Z3_mk_app(ctx(), decl, 0, {});
}

expr expr::mkQuantVar(unsigned i, Z3_sort sort) {
  return Z3_mk_bound(ctx(), i, sort);
}

expr expr::mkVar(const char *name, const expr &type) {
  C2(type);
  return ::mkVar(name, type.sort());
}

expr expr::mkVar(const char *name, unsigned bits) {
  return ::mkVar(name, mkBVSort(bits));
}

expr expr::mkBoolVar(const char *name) {
  return ::mkVar(name, Z3_mk_bool_sort(ctx()));
}

expr expr::mkHalfVar(const char *name) {
  return ::mkVar(name, Z3_mk_fpa_sort_half(ctx()));
}

expr expr::mkFloatVar(const char *name) {
  return ::mkVar(name, Z3_mk_fpa_sort_single(ctx()));
}

expr expr::mkDoubleVar(const char *name) {
  return ::mkVar(name, Z3_mk_fpa_sort_double(ctx()));
}

expr expr::mkFreshVar(const char *prefix, const expr &type) {
  C2(type);
  return Z3_mk_fresh_const(ctx(), prefix, type.sort());
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

bool expr::isBool() const {
  C();
  return Z3_get_sort_kind(ctx(), sort()) == Z3_BOOL_SORT;
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
  C();
  return eq(mkUInt(0, sort()));
}

bool expr::isOne() const {
  C();
  return eq(mkUInt(1, sort()));
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

bool expr::isSigned() const {
  C();
  auto bit = bits() - 1;
  return extract(bit, bit).eq(mkUInt(1, 1));
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

bool expr::isIf(expr &cond, expr &then, expr &els) const {
  if (auto app = isAppOf(Z3_OP_ITE)) {
    cond = Z3_get_app_arg(ctx(), app, 0);
    then = Z3_get_app_arg(ctx(), app, 1);
    els = Z3_get_app_arg(ctx(), app, 2);
    return true;
  }
  return false;
}

bool expr::isConcat(expr &a, expr &b) const {
  if (auto app = isAppOf(Z3_OP_CONCAT)) {
    assert(Z3_get_domain_size(ctx(), decl()) == 2);
    a = Z3_get_app_arg(ctx(), app, 0);
    b = Z3_get_app_arg(ctx(), app, 1);
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

bool expr::isNot(expr &neg) const {
  if (auto app = isAppOf(Z3_OP_NOT)) {
    neg = Z3_get_app_arg(ctx(), app, 0);
    return true;
  }
  return false;
}

bool expr::isConstArray(expr &val) const {
  if (auto app = isAppOf(Z3_OP_CONST_ARRAY)) {
    val = Z3_get_app_arg(ctx(), app, 0);
    return true;
  }
  return false;
}

bool expr::isStore(expr &array, expr &idx, expr &val) const {
  if (auto app = isAppOf(Z3_OP_STORE)) { // store(array, idx, val)
    array = Z3_get_app_arg(ctx(), app, 0);
    idx = Z3_get_app_arg(ctx(), app, 1);
    val = Z3_get_app_arg(ctx(), app, 2);
    return true;
  }
  return false;
}

unsigned expr::min_leading_zeros() const {
  expr a, b;
  uint64_t n;
  if (isConcat(a, b)) {
    return a.min_leading_zeros();
  } else if (isUInt(n)) {
    return num_leading_zeros(n) - (64 - bits());
  }
  return 0;
}

expr expr::binop_commutative(const expr &rhs,
                             Z3_ast (*op)(Z3_context, Z3_ast, Z3_ast),
                             bool (expr::*identity)() const,
                             bool (expr::*absorvent)() const) const {
  if ((this->*absorvent)() || (rhs.*identity)())
    return *this;

  if ((rhs.*absorvent)() || (this->*identity)())
    return rhs;

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

#define binopc(op, identity, absorvent)                             \
  binop_commutative(rhs, op, &expr::identity, &expr::absorvent)

expr expr::operator+(const expr &rhs) const {
  return binopc(Z3_mk_bvadd, isZero, alwaysFalse);
}

expr expr::operator-(const expr &rhs) const {
  C();
  if (eq(rhs))
    return mkUInt(0, sort());
  return *this + mkInt(-1, sort()) * rhs;
}

expr expr::operator*(const expr &rhs) const {
  return binopc(Z3_mk_bvmul, isOne, isZero);
}

expr expr::sdiv(const expr &rhs) const {
  if (eq(rhs))
    return mkUInt(1, sort());

  if (rhs.isZero())
    return rhs;

  if (isSMin() && rhs.isAllOnes())
    return mkUInt(0, sort());

  return binop_fold(rhs, Z3_mk_bvsdiv);
}

expr expr::udiv(const expr &rhs) const {
  if (eq(rhs))
    return mkUInt(1, sort());

  if (rhs.isZero())
    return rhs;

  return binop_fold(rhs, Z3_mk_bvudiv);
}

expr expr::srem(const expr &rhs) const {
  if (eq(rhs) || (isSMin() && rhs.isAllOnes()))
    return mkUInt(0, sort());

  if (rhs.isZero())
    return rhs;

  return binop_fold(rhs, Z3_mk_bvsrem);
}

expr expr::urem(const expr &rhs) const {
  C();
  if (eq(rhs))
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

expr expr::add_no_soverflow(const expr &rhs) const {
  if (min_leading_zeros() >= 2 && rhs.min_leading_zeros() >= 2)
    return true;
  return sext(1) + rhs.sext(1) == (*this + rhs).sext(1);
}

expr expr::add_no_uoverflow(const expr &rhs) const {
  if (min_leading_zeros() >= 1 && rhs.min_leading_zeros() >= 1)
    return true;

  int64_t n;
  if (isInt(n))
    return rhs.ule(-n - 1);
  if (rhs.isInt(n))
    return ule(-n - 1);

  auto bw = bits();
  return (zext(1) + rhs.zext(1)).extract(bw, bw) == 0;
}

expr expr::sub_no_soverflow(const expr &rhs) const {
  if (min_leading_zeros() >= 1 && rhs.min_leading_zeros() >= 1)
    return true;
  return sext(1) - rhs.sext(1) == (*this - rhs).sext(1);
}

expr expr::sub_no_uoverflow(const expr &rhs) const {
  auto bw = bits();
  return (zext(1) - rhs.zext(1)).extract(bw, bw) == 0;
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
  auto width = mkUInt(a.bits(), a.bits());
  expr c_mod_width = c.urem(width);
  return a << c_mod_width | b.lshr(width - c_mod_width);
}

expr expr::fshr(const expr &a, const expr &b, const expr &c) {
  C2(a);
  auto width = mkUInt(a.bits(), a.bits());
  expr c_mod_width = c.urem(width);
  return a << (width - c_mod_width) | b.lshr(c_mod_width);
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

expr expr::cttz() const {
  C();
  auto nbits = bits();

  auto cond = mkUInt(nbits, nbits);
  for (int i = nbits - 1; i >= 0; --i) {
    cond = mkIf(extract(i, i) == 1u, mkUInt(i, nbits), cond);
  }

  return cond;
}

expr expr::ctlz() const {
  C();
  auto nbits = bits();

  auto cond = mkUInt(nbits, nbits);
  for (unsigned i = 0; i < nbits; ++i) {
    cond = mkIf(extract(i, i) == 1u, mkUInt(nbits - 1 - i, nbits), cond);
  }

  return cond;
}

expr expr::ctpop() const {
  C();
  auto nbits = bits();

  auto res = mkUInt(0, nbits);
  for (unsigned i = 0; i < nbits; ++i) {
    res = res + extract(i, i).zext(nbits - 1);
  }

  return res;
}

expr expr::isNaN() const {
  return unop_fold(Z3_mk_fpa_is_nan);
}

expr expr::isInf() const {
  return unop_fold(Z3_mk_fpa_is_infinite);
}

expr expr::isFPZero() const {
  return unop_fold(Z3_mk_fpa_is_zero);
}

expr expr::isFPNeg() const {
  return unop_fold(Z3_mk_fpa_is_negative);
}

expr expr::isFPNegZero() const {
  return isFPZero() && isFPNeg();
}

// TODO: make rounding mode customizable
expr expr::fadd(const expr &rhs) const {
  C(rhs);
  auto rm = Z3_mk_fpa_round_nearest_ties_to_even(ctx());
  return Z3_mk_fpa_add(ctx(), rm, ast(), rhs());
}

expr expr::fsub(const expr &rhs) const {
  C(rhs);
  auto rm = Z3_mk_fpa_round_nearest_ties_to_even(ctx());
  return Z3_mk_fpa_sub(ctx(), rm, ast(), rhs());
}

expr expr::fmul(const expr &rhs) const {
  C(rhs);
  auto rm = Z3_mk_fpa_round_nearest_ties_to_even(ctx());
  return Z3_mk_fpa_mul(ctx(), rm, ast(), rhs());
}

expr expr::fdiv(const expr &rhs) const {
  C(rhs);
  auto rm = Z3_mk_fpa_round_nearest_ties_to_even(ctx());
  return Z3_mk_fpa_div(ctx(), rm, ast(), rhs());
}

expr expr::fneg() const {
  return unop_fold(Z3_mk_fpa_neg);
}

expr expr::foeq(const expr &rhs) const {
  return ford(rhs) && binop_commutative(rhs, Z3_mk_fpa_eq);
}

expr expr::fogt(const expr &rhs) const {
  return ford(rhs) && binop_fold(rhs, Z3_mk_fpa_gt);
}

expr expr::foge(const expr &rhs) const {
  return ford(rhs) && binop_fold(rhs, Z3_mk_fpa_geq);
}

expr expr::folt(const expr &rhs) const {
  return ford(rhs) && binop_fold(rhs, Z3_mk_fpa_lt);
}

expr expr::fole(const expr &rhs) const {
  return ford(rhs) && binop_fold(rhs, Z3_mk_fpa_leq);
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
  return !ford(rhs());
}

static expr get_bool(const expr &e) {
  expr cond, then, els;
  if (e.isIf(cond, then, els)) {
    if ((then == 1).isTrue() && (els == 0).isTrue())
      return cond;
    if ((then == 0).isTrue() && (els == 1).isTrue())
      return !cond;
  }
  return {};
}

expr expr::operator&(const expr &rhs) const {
  if (eq(rhs))
    return *this;

  auto fold_extract = [](auto &a, auto &b) {
    uint64_t n;
    if (!a.isUInt(n) || n == 0 || n == numeric_limits<uint64_t>::max())
      return expr();

    auto lead  = num_leading_zeros(n);
    auto trail = num_trailing_zeros(n);

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
  return binopc(Z3_mk_bvand, isAllOnes, isZero);
}

expr expr::operator|(const expr &rhs) const {
  if (eq(rhs))
    return *this;

  if (bits() == 1) {
    if (auto a = get_bool(*this);
        a.isValid())
      if (auto b = get_bool(rhs);
        b.isValid())
        return (a || b).toBVBool();
  }

  return binopc(Z3_mk_bvor, isZero, isAllOnes);
}

expr expr::operator^(const expr &rhs) const {
  if (eq(rhs))
    return mkUInt(0, sort());
  if (isAllOnes())
    return bits() == 1 ? (rhs == 0).toBVBool() : ~rhs;
  if (rhs.isAllOnes())
    return bits() == 1 ? (*this == 0).toBVBool() : ~*this;
  return binopc(Z3_mk_bvxor, isZero, alwaysFalse);
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

expr expr::operator==(const expr &rhs) const {
  if (eq(rhs))
    return true;
  if (isConst()) {
    if (rhs.isConst())
      return false;
    return rhs == *this;
  }
  if (rhs.isTrue())
    return *this;
  if (rhs.isFalse())
    return !*this;

  if (auto app = isAppOf(Z3_OP_CONCAT)) {
    unsigned num_args = Z3_get_app_num_args(ctx(), app);
    if (rhs.isConst()) {
      set<expr> eqs;
      unsigned high = bits();
      for (unsigned i = 0; i < num_args; ++i) {
        expr arg = Z3_get_app_arg(ctx(), app, i);
        unsigned low = high - arg.bits();
        eqs.emplace(arg == rhs.extract(high - 1, low));
        high = low;
      }
      return mk_and(eqs);
    }

    if (auto app_rhs = rhs.isAppOf(Z3_OP_CONCAT);
        app_rhs != nullptr &&
        num_args == Z3_get_app_num_args(ctx(), app_rhs)) {
      set<expr> eqs;
      bool ok = true;
      for (unsigned i = 0; i < num_args; ++i) {
        expr lhs = Z3_get_app_arg(ctx(), app, i);
        expr rhs = Z3_get_app_arg(ctx(), app_rhs, i);
        if (lhs.bits() != rhs.bits()) {
          ok = false;
          break;
        }
        eqs.emplace(lhs == rhs);
      }
      if (ok)
        return mk_and(eqs);
    }
  }

  expr c, t, e;
  if (isIf(c, t, e)) {
#if 0
    // TODO: benchmark
    // (= (ite c t e) (ite c x y)) -> (ite c (= t x) (= e y))
    if (auto rhs_app = rhs.isAppOf(Z3_OP_ITE)) {
      expr c2 = Z3_get_app_arg(ctx(), rhs_app, 0);
      if (c.eq(c2))
        return mkIf(c,
                    t == Z3_get_app_arg(ctx(), rhs_app, 1),
                    e == Z3_get_app_arg(ctx(), rhs_app, 2));
    }
#endif

    // (= (ite c t e) x) -> (ite c (= t x) (= e x))
    if (rhs.isConst() || (t.isConst() && e.isConst()))
      return mkIf(c, t == rhs, e == rhs);

  } else if (rhs.isAppOf(Z3_OP_ITE)) {
    return rhs == *this;
  }
  return binop_commutative(rhs, Z3_mk_eq);
}

expr expr::operator!=(const expr &rhs) const {
  return !(*this == rhs);
}

expr expr::operator&&(const expr &rhs) const {
  C(rhs);
  if (eq(rhs) || isFalse() || rhs.isTrue())
    return *this;
  if (isTrue() || rhs.isFalse())
    return rhs;

  Z3_ast args[] = { ast(), rhs() };
  return Z3_mk_and(ctx(), 2, args);
}

expr expr::operator||(const expr &rhs) const {
  C(rhs);
  if (eq(rhs) || rhs.isFalse() || isTrue())
    return *this;
  if (rhs.isTrue() || isFalse())
    return rhs;

  Z3_ast args[] = { ast(), rhs() };
  return Z3_mk_or(ctx(), 2, args);
}

void expr::operator&=(const expr &rhs) {
  if (!isValid() || eq(rhs) || isFalse() || rhs.isTrue()) {
    // do nothing
  } else if (!rhs.isValid() || rhs.isFalse() || isTrue()) {
    *this = rhs;
  } else {
    *this = *this && rhs;
  }
}

void expr::operator|=(const expr &rhs) {
  if (!isValid() || eq(rhs) || rhs.isFalse() || isTrue()) {
    // do nothing
  } else if (!rhs.isValid() || isFalse() || rhs.isTrue()) {
    *this = rhs;
  } else {
    *this = *this || rhs;
  }
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
  return !ule(rhs);
}

expr expr::sle(const expr &rhs) const {
  if (eq(rhs))
    return true;

  return binop_fold(rhs, Z3_mk_bvsle);
}

expr expr::slt(const expr &rhs) const {
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
  return binop_fold(rhs, Z3_mk_concat);
}

expr expr::concat_zeros(unsigned bits) const {
  return bits ? concat(expr::mkUInt(0, bits)) : *this;
}

expr expr::extract(unsigned high, unsigned low) const {
  C();
  assert(high >= low && high < bits());

  if (low == 0 && high == bits()-1)
    return *this;

  {
    expr sub;
    unsigned high_2, low_2;
    if (isExtract(sub, high_2, low_2))
      return sub.extract(high + low_2, low + low_2);
  }
  {
    expr a, b;
    if (isConcat(a, b)) {
      auto b_bw = b.bits();
      if (high < b_bw)
        return b.extract(high, low);
      if (low >= b_bw)
        return a.extract(high - b_bw, low - b_bw);
    }
  }
  {
    expr cond, then, els;
    if (isIf(cond, then, els) && then.isConst() && els.isConst()) {
      return mkIf(cond, then.extract(high, low), els.extract(high, low));
    }
  }
  return simplify_const(Z3_mk_extract(ctx(), high, low, ast()), *this);
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

expr expr::float2Float(const expr &type) const {
  C(type);
  auto rm = Z3_mk_fpa_round_nearest_ties_to_even(ctx());
  return Z3_mk_fpa_to_fp_float(ctx(), rm, ast(), type.sort());
}

expr expr::fp2sint(unsigned bits) const {
  C();
  auto rm = Z3_mk_fpa_round_nearest_ties_to_even(ctx());
  return simplify_const(Z3_mk_fpa_to_sbv(ctx(), rm, ast(), bits), *this);
}

expr expr::fp2uint(unsigned bits) const {
  C();
  auto rm = Z3_mk_fpa_round_nearest_ties_to_even(ctx());
  return simplify_const(Z3_mk_fpa_to_ubv(ctx(), rm, ast(), bits), *this);
}

expr expr::sint2fp(const expr &type) const {
  C(type);
  auto rm = Z3_mk_fpa_round_nearest_ties_to_even(ctx());
  return simplify_const(Z3_mk_fpa_to_fp_signed(ctx(), rm, ast(), type.sort()),
                        *this);
}

expr expr::uint2fp(const expr &type) const {
  C(type);
  auto rm = Z3_mk_fpa_round_nearest_ties_to_even(ctx());
  return simplify_const(Z3_mk_fpa_to_fp_unsigned(ctx(), rm, ast(), type.sort()),
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
    if ((idx == str_idx).simplify().isTrue())
      return array.store(idx, val);

  } else if (isConstArray(str_val)) {
    if (str_val.eq(val))
      return *this;
  }
  return Z3_mk_store(ctx(), ast(), idx(), val());
}

expr expr::load(const expr &idx) const {
  C(idx);

  // TODO: add support for alias analysis plugin
  expr array, str_idx, val;
  if (isStore(array, str_idx, val)) { // store(array, idx, val)
    expr cmp = (idx == str_idx).simplify();
    if (cmp.isTrue())
      return val;
    if (cmp.isFalse())
      return array.load(idx);

  } else if (isConstArray(val)) {
    return val;

  } else if (Z3_get_ast_kind(ctx(), ast()) == Z3_QUANTIFIER_AST &&
             Z3_is_lambda(ctx(), ast())) {
    assert(Z3_get_quantifier_num_bound(ctx(), ast()) == 1);
    expr body = Z3_get_quantifier_body(ctx(), ast());
    if (body.isConst())
      return body;

    auto extract_load = [&idx](expr &e, const expr &var) {
      if (auto app = e.isAppOf(Z3_OP_SELECT)) { // load(array, idx)
        expr lambda_idx = Z3_get_app_arg(ctx(), app, 1);
        expr new_idx = lambda_idx.subst(var, idx).simplify();
        assert(!idx.isValid() || !lambda_idx.eq(new_idx));
        e = expr(Z3_get_app_arg(ctx(), app, 0)).load(new_idx);
        return true;
      }
      return false;
    };

    expr cond, then, els;
    if (body.isIf(cond, then, els)) {
      auto sort = Z3_get_quantifier_bound_sort(ctx(), ast(), 0);
      expr var = expr::mkQuantVar(0, sort);
      cond = cond.subst(var, idx).simplify();
      if (cond.isTrue()) {
        if (then.isConst())
          return then;
        if (extract_load(then, var))
          return then;
      }
      if (cond.isFalse()) {
        if (els.isConst())
          return els;
        if (extract_load(els, var))
          return els;
      }
    }
  }

  return Z3_mk_select(ctx(), ast(), idx());
}

expr expr::mkIf(const expr &cond, const expr &then, const expr &els) {
  C2(cond, then, els);
  if (cond.isTrue() || then.eq(els))
    return then;
  if (cond.isFalse())
    return els;

  if (then.isTrue())
    return cond || els;
  if (then.isFalse())
    return !cond && els;
  if (els.isTrue())
    return !cond || then;
  if (els.isFalse())
    return cond && then;

  expr notcond;
  Z3_ast c, t = then(), e = els();
  if (cond.isNot(notcond)) {
    c = notcond();
    swap(t, e);
  } else {
    c = cond();
  }
  return Z3_mk_ite(ctx(), c, t, e);
}

expr expr::mkForAll(const set<expr> &vars, expr &&val) {
  if (vars.empty() || val.isConst() || !val.isValid())
    return move(val);

  unique_ptr<Z3_app[]> vars_ast(new Z3_app[vars.size()]);
  unsigned i = 0;
  for (auto &v : vars) {
    vars_ast[i++] = (Z3_app)v();
  }
  return Z3_mk_forall_const(ctx(), 0, vars.size(), vars_ast.get(), 0, nullptr,
                            val());
}

expr expr::mkLambda(const set<expr> &vars, const expr &val) {
  C2(val);
  assert(!vars.empty());

  unique_ptr<Z3_app[]> vars_ast(new Z3_app[vars.size()]);
  unsigned i = 0;
  for (auto &v : vars) {
    vars_ast[i++] = (Z3_app)v();
  }
  return Z3_mk_lambda_const(ctx(), vars.size(), vars_ast.get(), val());
}

expr expr::simplify() const {
  C();
  return Z3_simplify(ctx(), ast());
}

expr expr::subst(const vector<pair<expr, expr>> &repls) const {
  C();
  unique_ptr<Z3_ast[]> from(new Z3_ast[repls.size()]);
  unique_ptr<Z3_ast[]> to(new Z3_ast[repls.size()]);

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

set<expr> expr::vars() const {
  C();
  set<expr> result;
  unordered_set<Z3_ast> todo{ ast() };
  unordered_set<Z3_ast> seen = todo;

  do {
    auto I = todo.begin();
    auto ast = *I;
    todo.erase(I);

    switch (Z3_get_ast_kind(ctx(), ast)) {
    case Z3_VAR_AST:
    case Z3_NUMERAL_AST:
      break;

    case Z3_QUANTIFIER_AST: {
      auto body = Z3_get_quantifier_body(ctx(), ast);
      if (seen.emplace(body).second)
        todo.emplace(body);
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
          todo.emplace(arg);
      }
      break;
    }
    default:
      UNREACHABLE();
    }
  } while (!todo.empty());

  return result;
}

void expr::printUnsigned(ostream &os) const {
  os << numeral_string();
}

void expr::printSigned(ostream &os) const {
  if (isSigned()) {
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

ostream& operator<<(ostream &os, const expr &e) {
  return os << (e.isValid() ? Z3_ast_to_string(ctx(), e()) : "(null)");
}

bool expr::operator<(const expr &rhs) const {
  C(rhs);
  assert((id() == rhs.id()) == eq(rhs));
  return id() < rhs.id();
}

unsigned expr::id() const {
  return Z3_get_ast_id(ctx(), ast());
}

unsigned expr::hash() const {
  return Z3_get_ast_hash(ctx(), ast());
}

}
