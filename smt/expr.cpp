// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/ctx.h"
#include "smt/expr.h"
#include <cassert>
#include <z3.h>

#define DEBUG_Z3_RC 0

#if DEBUG_Z3_RC
# include <iostream>
#endif

using namespace std;

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
    Z3_inc_ref(ctx(), ast());
  } else {
    assert(0 && "TODO");
  }
}

Z3_sort expr::sort() const {
  return Z3_get_sort(ctx(), ast());
}

Z3_app expr::isApp() const {
  C();
  auto z3_ast = ast();
  if (Z3_is_app(ctx(), z3_ast))
    return Z3_to_app(ctx(), z3_ast);
  return nullptr;
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
  return mkUInt(n, mkBVSort(bits));
}

expr expr::mkInt(int64_t n, Z3_sort sort) {
  return Z3_mk_int64(ctx(), n, sort);
}

expr expr::mkInt(int64_t n, unsigned bits) {
  return mkInt(n, mkBVSort(bits));
}

expr expr::mkConst(Z3_func_decl decl) {
  return Z3_mk_app(ctx(), decl, 0, {});
}

expr expr::mkVar(const char *name, unsigned bits) {
  return ::mkVar(name, mkBVSort(bits));
}

expr expr::mkBoolVar(const char *name) {
  return ::mkVar(name, Z3_mk_bool_sort(ctx()));
}

expr expr::IntMin(unsigned bits) {
  assert(bits > 0);
  static_assert(sizeof(unsigned long long) == 8);
  if (bits <= 64)
    return mkUInt(1ull << (bits - 1), bits);
  return mkUInt(1, bits) << mkUInt(bits - 1, bits);
}

expr expr::IntMax(unsigned bits) {
  assert(bits > 0);
  static_assert(sizeof(unsigned long long) == 8);
  if (bits <= 64)
    return mkUInt(-1ull >> (65u - bits), bits);
  return mkInt(-1, bits).lshr(mkUInt(1, bits));
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
  return isUInt(n) && n == (1ull << (bits() - 1));
}

bool expr::isSMax() const {
  uint64_t n = 0;
  return isUInt(n) && n == ((uint64_t)INT64_MAX >> (64 - bits()));
}

unsigned expr::bits() const {
  return Z3_get_bv_sort_size(ctx(), sort());
}

bool expr::isUInt(uint64_t &n) const {
  C();
  return Z3_get_numeral_uint64(ctx(), ast(), &n) == Z3_TRUE;
}

bool expr::isInt(int64_t &n) const {
  C();
  if (Z3_get_numeral_int64(ctx(), ast(), &n) == Z3_FALSE)
    return false;

  auto bw = bits();
  if (bw < 64)
    n = (int64_t)((uint64_t)n << (64 - bw)) >> (64 - bw);
  return true;
}

bool expr::isNot(expr &neg) const {
  C();
  auto app = isApp();
  if (!app)
    return false;

  auto decl = Z3_get_app_decl(ctx(), app);
  if (Z3_get_decl_kind(ctx(), decl) != Z3_OP_NOT)
    return false;

  neg = Z3_get_app_arg(ctx(), app, 0);
  return true;
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
  if (bits() <= 64 && isInt(a) && rhs.isInt(b)) {
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

expr expr::add_no_soverflow(const expr &rhs) const {
  return sext(1) + rhs.sext(1) == (*this + rhs).sext(1);
}

expr expr::add_no_uoverflow(const expr &rhs) const {
  return zext(1) + rhs.zext(1) == (*this + rhs).zext(1);
}

expr expr::sub_no_soverflow(const expr &rhs) const {
  return sext(1) - rhs.sext(1) == (*this - rhs).sext(1);
}

expr expr::sub_no_uoverflow(const expr &rhs) const {
  return zext(1) - rhs.zext(1) == (*this - rhs).zext(1);
}

expr expr::mul_no_soverflow(const expr &rhs) const {
  C(rhs);
  auto bw = bits();
  return sext(bw) * rhs.sext(bw) == (*this * rhs).sext(bw);
}

expr expr::mul_no_uoverflow(const expr &rhs) const {
  C(rhs);
  auto bw = bits();
  return (zext(bw) * rhs.zext(bw)).extract(2 * bw - 1, bw) == mkUInt(0, bw);
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

  return Z3_mk_bvlshr(ctx(), ast(), rhs());
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

expr expr::operator&(const expr &rhs) const {
  if (eq(rhs))
    return *this;
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
    return rhs.isZero() ? false : ule(mkUInt(n - 1, sort()));

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

expr expr::sext(unsigned amount) const {
  assert(amount > 0);
  C();
  int64_t n;
  if (isInt(n))
    return mkInt(n, bits() + amount);
  return Z3_mk_sign_ext(ctx(), amount, ast());
}

expr expr::zext(unsigned amount) const {
  assert(amount > 0);
  uint64_t n;
  if (isUInt(n))
    return mkUInt(n, bits() + amount);
  return mkUInt(0, amount).concat(*this);
}

expr expr::trunc(unsigned tobw) const {
  return extract(tobw-1, 0);
}

expr expr::concat(const expr &rhs) const {
  C(rhs);
  return Z3_mk_concat(ctx(), ast(), rhs());
}

expr expr::extract(unsigned high, unsigned low) const {
  C();
  assert(high >= low && high < bits());
  return Z3_mk_extract(ctx(), high, low, ast());
}

expr expr::mkIf(const expr &cond, const expr &then, const expr &els) {
  C2(cond, then, els);
  if (cond.isTrue() || then.eq(els))
    return then;
  if (cond.isFalse())
    return els;

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

ostream& operator<<(ostream &os, const expr &e) {
  if (!e.isValid())
    return os << "(null)";
  return os << Z3_ast_to_string(ctx(), e());
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
