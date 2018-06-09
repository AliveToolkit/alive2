// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/solver.h"
#include "smt/ctx.h"
#include "util/compiler.h"
#include "util/config.h"
#include <z3.h>

using namespace smt;
using namespace util;
using namespace std;

static unsigned num_queries = 0;
static unsigned num_skips = 0;
static unsigned num_invalid = 0;
static unsigned num_trivial = 0;
static unsigned num_sats = 0;
static unsigned num_unsats = 0;
static unsigned num_unknown = 0;

namespace smt {

Model::Model(Z3_model m) : m(m) {
  Z3_model_inc_ref(ctx(), m);
}

Model::~Model() {
  if (m)
    Z3_model_dec_ref(ctx(), m);
}

void Model::operator=(Model &&other) {
  this->~Model();
  m = 0;
  swap(other.m, m);
}

expr Model::operator[](const expr &var) const {
  Z3_ast val;
  if (Z3_model_eval(ctx(), m, var(), Z3_TRUE, &val) == Z3_FALSE)
    return {};
  return val;
}

uint64_t Model::getUInt(const expr &var) const {
  uint64_t n;
  ENSURE((*this)[var].isUInt(n));
  return n;
}

int64_t Model::getInt(const expr &var) const {
  int64_t n;
  ENSURE((*this)[var].isInt(n));
  return n;
}

pair<expr, expr> Model::iterator::operator*(void) const {
  auto decl = Z3_model_get_const_decl(ctx(), m, idx);
  return { expr::mkConst(decl), Z3_model_get_const_interp(ctx(), m, decl) };
}

Model::iterator Model::begin() const {
  return { m, 0 };
}

Model::iterator Model::end() const {
  return { nullptr, Z3_model_get_num_consts(ctx(), m) };
}


SolverPop::~SolverPop() {
  Z3_solver_pop(ctx(), s.s, 1);
}


Solver::Solver() {
  // FIXME: implement a tactic
  s = Z3_mk_simple_solver(ctx());
  Z3_solver_inc_ref(ctx(), s);
}

Solver::~Solver() {
  Z3_solver_dec_ref(ctx(), s);
}

void Solver::add(const expr &e) {
  if (e.isValid()) {
    Z3_solver_assert(ctx(), s, e());
  } else {
    valid = false;
  }
}

void Solver::block(const Model &m) {
  expr c(false);
  for (const auto &[var, val] : m) {
    c |= var != val;
  }
  add(c);
}

SolverPop Solver::push() {
  Z3_solver_push(ctx(), s);
  return *this;
}

void Solver::reset() {
  Z3_solver_reset(ctx(), s);
}

Result Solver::check() const {
  if (config::skip_smt) {
    ++num_skips;
    return Result::UNKNOWN;
  }

  if (!valid) {
    ++num_invalid;
    return Result::UNKNOWN;
  }

  ++num_queries;

  switch (Z3_solver_check(ctx(), s)) {
  case Z3_L_FALSE:
    ++num_unsats;
    return Result::UNSAT;
  case Z3_L_TRUE:
    ++num_sats;
    return { Z3_solver_get_model(ctx(), s), Result::SAT };
  case Z3_L_UNDEF:
    ++num_unknown;
    return Result::UNKNOWN;
  default:
    UNREACHABLE();
  }
}

Result Solver::check(const expr &e) {
  if (!e.isValid()) {
    ++num_invalid;
    return Result::UNKNOWN;
  }

  if (e.isFalse()) {
    ++num_trivial;
    return Result::UNSAT;
  }

  // FIXME: benchmark: is push/pop the best?
  auto pop = push();
  add(e);
  return check();
}

void Solver::check(initializer_list<E> queries) {
  for (auto &[q, error] : queries) {
    auto res = check(q);
    if (res.isSat()) {
      error(res.m);
      return;
    }
  }
}


EnableSMTQueriesTMP::EnableSMTQueriesTMP() : old(config::skip_smt) {
  config::skip_smt = false;
}

EnableSMTQueriesTMP::~EnableSMTQueriesTMP() {
  config::skip_smt = old;
}

}
