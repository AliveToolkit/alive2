#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include <cstdint>
#include <ostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

typedef struct _Z3_context* Z3_context;
typedef struct _Z3_func_decl* Z3_decl;
typedef struct _Z3_app* Z3_app;
typedef struct _Z3_ast* Z3_ast;
typedef struct _Z3_sort* Z3_sort;
typedef struct _Z3_func_decl* Z3_func_decl;

namespace smt {

class expr {
  uintptr_t ptr;

  expr(Z3_ast ast);
  bool isZ3Ast() const;
  Z3_ast ast() const;
  Z3_ast operator()() const { return ast(); }
  void incRef();
  void decRef();

  Z3_sort sort() const;
  Z3_decl decl() const;
  Z3_app isApp() const;
  Z3_app isAppOf(int app_type) const;

  expr binop_commutative(const expr &rhs,
                         Z3_ast(*op)(Z3_context, Z3_ast, Z3_ast),
                         bool (expr::*identity)() const,
                         bool (expr::*absorvent)() const) const;
  expr binop_commutative(const expr &rhs,
                         Z3_ast(*op)(Z3_context, Z3_ast, Z3_ast)) const;

  expr unop_fold(Z3_ast(*op)(Z3_context, Z3_ast)) const;
  expr binop_fold(const expr &rhs,
                  Z3_ast(*op)(Z3_context, Z3_ast, Z3_ast)) const;

  template <typename... Exprs>
  static expr simplify_const(expr &&e, const expr &input,
                             const Exprs &... inputs) {
    if (input.isConst())
      return simplify_const(std::move(e), inputs...);
    return std::move(e);
  }
  static expr simplify_const(expr &&e) { return e.simplify(); }

  bool alwaysFalse() const { return false; }

  static Z3_ast mkTrue();
  static Z3_ast mkFalse();
  static expr mkUInt(uint64_t n, Z3_sort sort);
  static expr mkInt(int64_t n, Z3_sort sort);
  static expr mkConst(Z3_func_decl decl);
  static expr mkQuantVar(unsigned i, Z3_sort sort);

public:
  expr() : ptr(0) {}

  expr(expr &&other) : ptr(0) {
    std::swap(ptr, other.ptr);
  }

  expr(const expr &other);
  expr(bool val) : expr(val ? mkTrue() : mkFalse()) {}
  ~expr();

  void operator=(expr &&other);
  void operator=(const expr &other);

  static expr mkUInt(uint64_t n, unsigned bits);
  static expr mkInt(int64_t n, unsigned bits);
  static expr mkInt(int64_t n, const expr &type);
  static expr mkInt(const char *n, unsigned bits);
  static expr mkFloat(double n, const expr &type);
  static expr mkHalf(float n);
  static expr mkFloat(float n);
  static expr mkDouble(double n);
  static expr mkNaN(const expr &type);
  static expr mkNumber(const char *n, const expr &type);
  static expr mkVar(const char *name, const expr &type);
  static expr mkVar(const char *name, unsigned bits);
  static expr mkBoolVar(const char *name);
  static expr mkHalfVar(const char *name);
  static expr mkFloatVar(const char *name);
  static expr mkDoubleVar(const char *name);
  static expr mkFreshVar(const char *prefix, const expr &type);

  static expr IntSMin(unsigned bits);
  static expr IntSMax(unsigned bits);
  static expr IntUMax(unsigned bits);

  // structural equivalence
  bool eq(const expr &rhs) const;

  bool isValid() const { return ptr != 0; }

  bool isConst() const;
  bool isBool() const;
  bool isTrue() const;
  bool isFalse() const;
  bool isZero() const;
  bool isOne() const;
  bool isAllOnes() const;
  bool isSMin() const;
  bool isSMax() const;
  bool isSigned() const;

  unsigned bits() const;
  bool isUInt(uint64_t &n) const;
  bool isInt(int64_t &n) const;

  bool isEq(expr &lhs, expr &rhs);
  bool isIf(expr &cond, expr &then, expr &els) const;
  bool isConcat(expr &a, expr &b) const;
  bool isExtract(expr &e, unsigned &high, unsigned &low) const;
  bool isNot(expr &neg) const;
  bool isConstArray(expr &val) const;
  bool isStore(expr &array, expr &idx, expr &val) const;

  bool isNaNCheck(expr &fp) const;
  bool isfloat2BV(expr &fp) const;

  // best effort; returns number of statically known bits
  unsigned min_leading_zeros() const;

  expr operator+(const expr &rhs) const;
  expr operator-(const expr &rhs) const;
  expr operator*(const expr &rhs) const;
  expr sdiv(const expr &rhs) const;
  expr udiv(const expr &rhs) const;
  expr srem(const expr &rhs) const;
  expr urem(const expr &rhs) const;

  // saturating arithmetic
  expr sadd_sat(const expr &rhs) const;
  expr uadd_sat(const expr &rhs) const;
  expr ssub_sat(const expr &rhs) const;
  expr usub_sat(const expr &rhs) const;

  expr add_no_soverflow(const expr &rhs) const;
  expr add_no_uoverflow(const expr &rhs) const;
  expr sub_no_soverflow(const expr &rhs) const;
  expr sub_no_uoverflow(const expr &rhs) const;
  expr mul_no_soverflow(const expr &rhs) const;
  expr mul_no_uoverflow(const expr &rhs) const;
  expr sdiv_exact(const expr &rhs) const;
  expr udiv_exact(const expr &rhs) const;

  expr operator<<(const expr &rhs) const;
  expr ashr(const expr &rhs) const;
  expr lshr(const expr &rhs) const;

  static expr fshl(const expr &a, const expr &b, const expr &c);
  static expr fshr(const expr &a, const expr &b, const expr &c);

  expr shl_no_soverflow(const expr &rhs) const;
  expr shl_no_uoverflow(const expr &rhs) const;
  expr ashr_exact(const expr &rhs) const;
  expr lshr_exact(const expr &rhs) const;

  expr log2(unsigned bw_output) const;
  expr bswap() const;
  expr bitreverse() const;
  expr cttz() const;
  expr ctlz() const;
  expr ctpop() const;

  expr isNaN() const;
  expr isInf() const;
  expr isFPZero() const;
  expr isFPNeg() const;
  expr isFPNegZero() const;

  expr fadd(const expr &rhs) const;
  expr fsub(const expr &rhs) const;
  expr fmul(const expr &rhs) const;
  expr fdiv(const expr &rhs) const;
  expr fneg() const;

  expr foeq(const expr &rhs) const;
  expr fogt(const expr &rhs) const;
  expr foge(const expr &rhs) const;
  expr folt(const expr &rhs) const;
  expr fole(const expr &rhs) const;
  expr fone(const expr &rhs) const;
  expr ford(const expr &rhs) const;
  expr fueq(const expr &rhs) const;
  expr fugt(const expr &rhs) const;
  expr fuge(const expr &rhs) const;
  expr fult(const expr &rhs) const;
  expr fule(const expr &rhs) const;
  expr fune(const expr &rhs) const;
  expr funo(const expr &rhs) const;

  expr operator&(const expr &rhs) const;
  expr operator|(const expr &rhs) const;
  expr operator^(const expr &rhs) const;

  expr operator!() const;
  expr operator~() const;

  expr operator==(const expr &rhs) const;
  expr operator!=(const expr &rhs) const;

  expr operator&&(const expr &rhs) const;
  expr operator||(const expr &rhs) const;
  // the following are boolean only:
  void operator&=(const expr &rhs);
  void operator|=(const expr &rhs);

  static expr mk_and(const std::set<expr> &vals);
  static expr mk_or(const std::set<expr> &vals);

  expr implies(const expr &rhs) const;
  expr notImplies(const expr &rhs) const;

  expr ule(const expr &rhs) const;
  expr ult(const expr &rhs) const;
  expr uge(const expr &rhs) const;
  expr ugt(const expr &rhs) const;
  expr sle(const expr &rhs) const;
  expr slt(const expr &rhs) const;
  expr sge(const expr &rhs) const;
  expr sgt(const expr &rhs) const;

  expr ule(uint64_t rhs) const;
  expr ult(uint64_t rhs) const;
  expr uge(uint64_t rhs) const;
  expr ugt(uint64_t rhs) const;
  expr sle(int64_t rhs) const;
  expr sge(int64_t rhs) const;
  expr operator==(uint64_t rhs) const;
  expr operator!=(uint64_t rhs) const;

  expr sext(unsigned amount) const;
  expr zext(unsigned amount) const;
  expr trunc(unsigned tobw) const;
  expr sextOrTrunc(unsigned tobw) const;
  expr zextOrTrunc(unsigned tobw) const;

  expr concat(const expr &rhs) const;
  expr concat_zeros(unsigned bits) const;
  expr extract(unsigned high, unsigned low) const;

  expr toBVBool() const;
  expr float2BV() const;
  expr float2Real() const;
  expr BV2float(const expr &type) const;
  expr float2Float(const expr &type) const;

  expr fp2sint(unsigned bits) const;
  expr fp2uint(unsigned bits) const;
  expr sint2fp(const expr &type) const;
  expr uint2fp(const expr &type) const;

  // we don't expose SMT expr types, so range must be passed as a dummy value
  // of the desired type
  static expr mkUF(const char *name, const std::vector<expr> &args,
                   const expr &range);

  static expr mkArray(const char *name, const expr &domain, const expr &range);
  static expr mkConstArray(const expr &domain, const expr &value);

  expr store(const expr &idx, const expr &val) const;
  expr load(const expr &idx) const;

  static expr mkIf(const expr &cond, const expr &then, const expr &els);
  static expr mkForAll(const std::set<expr> &vars, expr &&val);
  static expr mkLambda(const std::set<expr> &vars, const expr &val);

  expr simplify() const;

  // replace v1 -> v2
  expr subst(const std::vector<std::pair<expr, expr>> &repls) const;
  expr subst(const expr &from, const expr &to) const;

  std::set<expr> vars() const;
  static std::set<expr> vars(const std::vector<const expr*> &exprs);

  // returns set of all possible leaf expressions (best-effort simplification)
  static std::vector<expr> allLeafs(const expr &e);

  void printUnsigned(std::ostream &os) const;
  void printSigned(std::ostream &os) const;
  void printHexadecimal(std::ostream &os) const;
  std::string numeral_string() const;
  std::string fn_name() const; // empty if not a function
  friend std::ostream &operator<<(std::ostream &os, const expr &e);

  // for container use only
  bool operator<(const expr &rhs) const;
  unsigned id() const;
  unsigned hash() const;


  template <typename... Exprs>
  static bool allValid(const expr &e, Exprs&&... exprs) {
    return e.isValid() && allValid(exprs...);
  }
  static bool allValid(const expr &e) { return e.isValid(); }
  static bool allValid() { return true; }

  friend class Solver;
  friend class Model;
};

}
