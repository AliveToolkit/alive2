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
  ~expr();
  ptr = 0;
  swap(ptr, other.ptr);
}

void expr::operator=(const expr &other) {
  ~expr();
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

expr expr::mkInt(const char *n, unsigned bits) {
  return bits ? Z3_mk_numeral(ctx(), n, mkBVSort(bits)) : expr();
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

expr expr::mkConst(Z3_func_decl decl) {
  return Z3_mk_app(ctx(), decl, 0, {});
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

expr expr::IntSMin(unsigned bits) {
  assert(bits > 0);
  static_assert(sizeof(unsigned long long) == 8);
  if (bits <= 64)
    return mkUInt(1ull << (bits - 1), bits);
  return mkUInt(1, 1).concat(mkUInt(0, bits-1));
}

expr expr::IntSMax(unsigned bits) {
  assert(bits > 0);
  static_assert(sizeof(unsigned long long) == 8);
  if (bits <= 64)
    return mkUInt(numeric_limits<uint64_t>::max() >> (65u - bits), bits);
  return mkUInt(0, 1).concat(mkInt(-1, bits-1));
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
  return Z3_is_numeral_ast(ctx(), ast());
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
  uint64_t n = 0;
  return isUInt(n) && n == 0;
}

bool expr::isOne() const {
  uint64_t n = 0;
  return isUInt(n) && n == 1;
}

bool expr::isAllOnes() const {
  int64_t n = 0;
  return isInt(n) && n == -1;
}

bool expr::isSMin() const {
  uint64_t n = 0;
  return bits() <= 64 && isUInt(n) && n == (1ull << (bits() - 1));
}

bool expr::isSMax() const {
  uint64_t n = 0;
  return bits() <= 64 && isUInt(n) &&
         n == ((uint64_t)INT64_MAX >> (64 - bits()));
}

bool expr::isSigned() const {
  auto bit = bits() - 1;
  return (extract(bit, bit) == 1).simplify().isTrue();
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
                             uint64_t (*native)(uint64_t, uint64_t),
                             Z3_ast (*z3)(Z3_context, Z3_ast, Z3_ast),
                             bool (expr::*identity)() const,
                             bool (expr::*absorvent)() const) const {
  C(rhs);
  expr r;
  if (binop_ufold(rhs, native, r))
    return r;

  if ((this->*absorvent)() || (rhs.*identity)())
    return *this;

  if ((rhs.*absorvent)() || (this->*identity)())
    return rhs;

  return binop_commutative(rhs, z3);
}

expr expr::binop_commutative(const expr &rhs,
                             Z3_ast(*z3)(Z3_context, Z3_ast, Z3_ast)) const {
  auto cmp = *this < rhs;
  auto a = ast(), b = rhs();
  return z3(ctx(), cmp ? a : b, cmp ? b : a);
}

bool expr::binop_sfold(const expr &rhs,
                       int64_t(*native)(int64_t, int64_t), expr &result) const {
  int64_t a, b;
  if (/*bits() <= 64 &&*/ isInt(a) && rhs.isInt(b)) {
    result = mkInt(native(a, b), sort());
    return true;
  }
  return false;
}

bool expr::binop_ufold(const expr &rhs,
                       uint64_t(*native)(uint64_t, uint64_t),
                       expr &result) const {
  uint64_t a, b;
  if (bits() <= 64 && isUInt(a) && rhs.isUInt(b)) {
    result = mkUInt(native(a, b), sort());
    return true;
  }
  return false;
}

#define binopc(native_op, z3, identity, absorvent)                             \
  binop_commutative(rhs, [](uint64_t a, uint64_t b) { return a native_op b; }, \
                    z3, &expr::identity, &expr::absorvent)

expr expr::operator+(const expr &rhs) const {
  return binopc(+, Z3_mk_bvadd, isZero, alwaysFalse);
}

expr expr::operator-(const expr &rhs) const {
  if (eq(rhs))
    return mkUInt(0, sort());
  return *this + mkInt(-1, sort()) * rhs;
}

expr expr::operator*(const expr &rhs) const {
  return binopc(*, Z3_mk_bvmul, isOne, isZero);
}

expr expr::sdiv(const expr &rhs) const {
  C(rhs);

  if (eq(rhs))
    return mkUInt(1, sort());

  if (rhs.isZero())
    return rhs;

  if (isSMin() && rhs.isAllOnes())
    return mkUInt(0, sort());

  expr r;
  if (binop_sfold(rhs, [](auto a, auto b) { return a / b; }, r))
    return r;

  return Z3_mk_bvsdiv(ctx(), ast(), rhs());
}

expr expr::udiv(const expr &rhs) const {
  C(rhs);

  if (eq(rhs))
    return mkUInt(1, sort());

  if (rhs.isZero())
    return rhs;

  expr r;
  if (binop_ufold(rhs, [](auto a, auto b) { return a / b; }, r))
    return r;

  return Z3_mk_bvudiv(ctx(), ast(), rhs());
}

expr expr::srem(const expr &rhs) const {
  C(rhs);

  if (eq(rhs) || (isSMin() && rhs.isAllOnes()))
    return mkUInt(0, sort());

  if (rhs.isZero())
    return rhs;

  expr r;
  if (binop_sfold(rhs, [](auto a, auto b) { return a % b; }, r))
    return r;

  return Z3_mk_bvsrem(ctx(), ast(), rhs());
}

expr expr::urem(const expr &rhs) const {
  C(rhs);

  if (eq(rhs))
    return mkUInt(0, sort());

  if (rhs.isZero())
    return rhs;

  expr r;
  if (binop_ufold(rhs, [](auto a, auto b) { return a % b; }, r))
    return r;

  return Z3_mk_bvurem(ctx(), ast(), rhs());
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
  C(rhs);
  auto bw = bits();
  return sext(bw) * rhs.sext(bw) == (*this * rhs).sext(bw);
}

expr expr::mul_no_uoverflow(const expr &rhs) const {
  C(rhs);
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
  C(rhs);
  if (isZero() || rhs.isZero())
    return *this;

  expr r;
  if (binop_ufold(rhs, [](auto a, auto b) { return a << b; }, r))
    return r;

  uint64_t shift;
  if (rhs.isUInt(shift)) {
    auto bw = bits();
    if (shift >= bw)
      return mkUInt(0, sort());
    return extract(bw-shift-1, 0).concat(mkUInt(0, shift));
  }

  return Z3_mk_bvshl(ctx(), ast(), rhs());
}

expr expr::ashr(const expr &rhs) const {
  C(rhs);
  if (isZero() || rhs.isZero())
    return *this;

  expr r;
  if (binop_sfold(rhs, [](auto a, auto b) { return a >> b; }, r))
    return r;

  return Z3_mk_bvashr(ctx(), ast(), rhs());
}

expr expr::lshr(const expr &rhs) const {
  C(rhs);
  if (isZero() || rhs.isZero())
    return *this;

  expr r;
  if (binop_ufold(rhs, [](auto a, auto b) { return a >> b; }, r))
    return r;

  uint64_t shift;
  if (rhs.isUInt(shift)) {
    auto bw = bits();
    if (shift >= bw)
      return mkUInt(0, sort());
    return mkUInt(0, shift).concat(extract(bw-1, shift));
  }

  return Z3_mk_bvlshr(ctx(), ast(), rhs());
}

expr expr::fshl(const expr &a, const expr &b, const expr &c) {
  C2(a);
  auto nbits = a.bits();
  expr c_mod_width = c.urem(mkUInt(nbits, nbits)).zext(nbits);
  expr res = a.concat(b) << c_mod_width;
  return res.extract(2 * nbits - 1, nbits); // upper half (MSB)
}

expr expr::fshr(const expr &a, const expr &b, const expr &c) {
  C2(a);
  auto nbits = a.bits();
  expr c_mod_width = c.urem(mkUInt(nbits, nbits)).zext(nbits);
  expr res = a.concat(b).lshr(c_mod_width);
  return res.extract(nbits - 1, 0); // lower half (LSB)
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
  for (unsigned i = 1; i < nbits; i++) {
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
  C();
  return Z3_mk_fpa_is_nan(ctx(), ast());
}

expr expr::isInf() const {
  C();
  return Z3_mk_fpa_is_infinite(ctx(), ast());
}

expr expr::isFPZero() const {
  C();
  return Z3_mk_fpa_is_zero(ctx(), ast());
}

expr expr::isFPNeg() const {
  C();
  return Z3_mk_fpa_is_negative(ctx(), ast());
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

expr expr::foeq(const expr &rhs) const {
  C(rhs);
  return ford(rhs) && Z3_mk_fpa_eq(ctx(), ast(), rhs());
}

expr expr::fogt(const expr &rhs) const {
  C(rhs);
  return ford(rhs) && Z3_mk_fpa_gt(ctx(), ast(), rhs());
}

expr expr::foge(const expr &rhs) const {
  C(rhs);
  return ford(rhs) && Z3_mk_fpa_geq(ctx(), ast(), rhs());
}

expr expr::folt(const expr &rhs) const {
  C(rhs);
  return ford(rhs) && Z3_mk_fpa_lt(ctx(), ast(), rhs());
}

expr expr::fole(const expr &rhs) const {
  C(rhs);
  return ford(rhs) && Z3_mk_fpa_leq(ctx(), ast(), rhs());
}

expr expr::fone(const expr &rhs) const {
  C(rhs);
  return ford(rhs) && !expr(Z3_mk_fpa_eq(ctx(), ast(), rhs()));
}

expr expr::ford(const expr &rhs) const {
  return !isNaN() && !rhs.isNaN();
}

expr expr::fueq(const expr &rhs) const {
  C(rhs);
  return funo(rhs) || Z3_mk_fpa_eq(ctx(), ast(), rhs());
}

expr expr::fugt(const expr &rhs) const {
  C(rhs);
  return funo(rhs) || Z3_mk_fpa_gt(ctx(), ast(), rhs());
}

expr expr::fuge(const expr &rhs) const {
  C(rhs);
  return funo(rhs) || Z3_mk_fpa_geq(ctx(), ast(), rhs());
}

expr expr::fult(const expr &rhs) const {
  C(rhs);
  return funo(rhs) || Z3_mk_fpa_lt(ctx(), ast(), rhs());
}

expr expr::fule(const expr &rhs) const {
  C(rhs);
  return funo(rhs) || Z3_mk_fpa_leq(ctx(), ast(), rhs());
}

expr expr::fune(const expr &rhs) const {
  C(rhs);
  return funo(rhs) || !expr(Z3_mk_fpa_eq(ctx(), ast(), rhs()));
}

expr expr::funo(const expr &rhs) const {
  return !ford(rhs());
}

expr expr::fneg() const {
  C();
  return Z3_mk_fpa_neg(ctx(), ast());
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
  }
  return binopc(&, Z3_mk_bvand, isAllOnes, isZero);
}

expr expr::operator|(const expr &rhs) const {
  if (eq(rhs))
    return *this;
  return binopc(|, Z3_mk_bvor, isZero, isAllOnes);
}

expr expr::operator^(const expr &rhs) const {
  if (eq(rhs))
    return mkUInt(0, sort());
  return binopc(^, Z3_mk_bvxor, isZero, alwaysFalse);
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
  C();
  int64_t n;
  if (isInt(n))
    return mkUInt(~n, sort());
  return mkInt(-1, sort()) - *this;
}

expr expr::operator==(const expr &rhs) const {
  C(rhs);
  if (eq(rhs))
    return true;
  if (isConst() && rhs.isConst())
    return false;

  if (auto app = isAppOf(Z3_OP_ITE)) {
    expr c = Z3_get_app_arg(ctx(), app, 0);
    expr t = Z3_get_app_arg(ctx(), app, 1);
    expr e = Z3_get_app_arg(ctx(), app, 2);

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
  C(rhs);
  if (eq(rhs) || isZero())
    return true;

  uint64_t a, b;
  if (isUInt(a) && rhs.isUInt(b))
    return a <= b;

  return Z3_mk_bvule(ctx(), ast(), rhs());
}

expr expr::ult(const expr &rhs) const {
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
  C(rhs);
  if (eq(rhs))
    return true;

  int64_t a, b;
  if (isInt(a) && rhs.isInt(b))
    return a <= b;

  return Z3_mk_bvsle(ctx(), ast(), rhs());
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

  int64_t n;
  if (isInt(n))
    return mkInt(n, bits() + amount);
  return Z3_mk_sign_ext(ctx(), amount, ast());
}

expr expr::zext(unsigned amount) const {
  if (amount == 0)
    return *this;

  uint64_t n;
  if (isUInt(n))
    return mkUInt(n, bits() + amount);
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
  C(rhs);

  auto rhs_bits = rhs.bits();
  auto bw = bits() + rhs_bits;
  uint64_t a, b;
  if (bw <= 64 && isUInt(a) && rhs.isUInt(b))
    return mkUInt((a << rhs_bits) | b, bw);

  return Z3_mk_concat(ctx(), ast(), rhs());
}

expr expr::extract(unsigned high, unsigned low) const {
  C();
  assert(high >= low && high < bits());

  if (low == 0 && high == bits()-1)
    return *this;

  uint64_t n;
  if (low < 64 && isUInt(n))
    return mkUInt(n >> low, high - low + 1);

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
  return Z3_mk_extract(ctx(), high, low, ast());
}

expr expr::seq_append(const expr &rhs) const {
  C();
  Z3_ast _args[2] = { ast(), rhs.toUnitSeq()() };
  return Z3_mk_seq_concat(ctx(), 2, _args);
}

expr expr::seq_at(unsigned idx) const {
  C();
  auto sort = Z3_mk_int_sort(ctx());
  return Z3_mk_seq_at(ctx(), ast(), mkUInt(idx, sort)());
}

expr expr::toBVBool() const {
  auto sort = mkBVSort(1);
  return mkIf(*this, mkUInt(1, sort), mkUInt(0, sort));
}

expr expr::float2BV() const {
  C();
  return Z3_mk_fpa_to_ieee_bv(ctx(), ast());
}

expr expr::float2Real() const {
  C();
  return Z3_mk_fpa_to_real(ctx(), ast());
}

expr expr::BV2float(const expr &type) const {
  C(type);
  return Z3_mk_fpa_to_fp_bv(ctx(), ast(), type.sort());
}

expr expr::toUnitSeq() const {
  C();
  return Z3_mk_seq_unit(ctx(), ast());
}

expr expr::mkUF(const char *name, const vector<expr> &args, const expr &range) {
  C2(range);
  auto num_args = args.size();
  vector<Z3_ast> z3_args;
  vector<Z3_sort> z3_sorts;
  z3_args.reserve(num_args);
  z3_sorts.reserve(num_args);

  for (auto &arg : args) {
    if (!arg.isValid())
      return {};
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

expr expr::store(const expr &idx, const expr &val) const {
  C(idx, val);
  return Z3_mk_store(ctx(), ast(), idx(), val());
}

expr expr::load(const expr &idx) const {
  C(idx);
  return Z3_mk_select(ctx(), ast(), idx());
}

expr expr::mkIf(const expr &cond, const expr &then, const expr &els) {
  C2(cond, then, els);
  if (cond.isTrue() || then.eq(els))
    return then;
  if (cond.isFalse())
    return els;

  if (then.isTrue() && els.isFalse())
    return cond;
  if (then.isFalse() && els.isTrue())
    return !cond;

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
  if (vars.empty() || val.isTrue() || val.isFalse() || !val.isValid())
    return move(val);

  unique_ptr<Z3_app[]> vars_ast(new Z3_app[vars.size()]);
  unsigned i = 0;
  for (auto &v : vars) {
    vars_ast[i++] = (Z3_app)v();
  }
  return Z3_mk_forall_const(ctx(), 0, vars.size(), vars_ast.get(), 0, nullptr,
                            val());
}

expr expr::mkLambda(const set<expr> &vars, expr &&val) {
  if (vars.empty() || val.isTrue() || val.isFalse() || !val.isValid())
    return move(val);

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
    (~*this + mkUInt(1, sort())).simplify().printUnsigned(os);
  } else {
    printUnsigned(os);
  }
}

void expr::printHexadecimal(ostream &os) const {
  auto rem = bits() % 4;
  os << (rem == 0 ? *this : zext(4 - rem)).simplify();
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
