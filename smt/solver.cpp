// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/solver.h"
#include "smt/ctx.h"
#include "util/compiler.h"
#include "util/config.h"
#include <cassert>
#include <iomanip>
#include <iostream>
#include <utility>
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

namespace {
class Tactic {
  Z3_tactic t;

  Tactic(Z3_tactic t) : t(t) {
    Z3_tactic_inc_ref(ctx(), t);
  }

  void destroy() {
    if (t)
      Z3_tactic_dec_ref(ctx(), t);
  }

public:
  Tactic() : t(nullptr) {}
  Tactic(const char *name) : Tactic(Z3_mk_tactic(ctx(), name)) {}

  Tactic(Tactic &&other) : t(nullptr) {
    swap(t, other.t);
  }

  void operator=(Tactic &&other) {
    destroy();
    t = nullptr;
    swap(t, other.t);
  }

  Tactic(initializer_list<Tactic> ts) : t(nullptr) {
    assert(ts.size() != 0);
    Tactic tmp = move(const_cast<Tactic&>(*ts.begin()));

    for (auto I = next(ts.begin()), E = ts.end(); I != E; ++I) {
      tmp = Tactic::mkThen(tmp, *I);
    }
    *this = move(tmp);
  }

  static Tactic mkThen(const Tactic &a, const Tactic &b) {
    return Z3_tactic_and_then(ctx(), a.t, b.t);
  }

  ~Tactic() { destroy(); }

  friend class smt::Solver;
};
}

static Tactic tactic;


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
  return Z3_model_eval(ctx(), m, var(), true, &val) ? val : expr();
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


static bool print_queries = false;
void solver_print_queries(bool yes) {
  print_queries = yes;
}

Solver::Solver() {
  s = Z3_mk_solver_from_tactic(ctx(), tactic.t);
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
    return Result::INVALID;
  }

  ++num_queries;
  if (print_queries)
    cout << "\nSMT query:\n" << Z3_solver_to_string(ctx(), s);

  switch (Z3_solver_check(ctx(), s)) {
  case Z3_L_FALSE:
    ++num_unsats;
    return Result::UNSAT;
  case Z3_L_TRUE:
    ++num_sats;
    return Z3_solver_get_model(ctx(), s);
  case Z3_L_UNDEF:
    ++num_unknown;
    return Result::UNKNOWN;
  default:
    UNREACHABLE();
  }
}

void Solver::check(initializer_list<E> queries) {
  for (auto &[q, error] : queries) {
    if (!q.isValid()) {
      ++num_invalid;
      error(Result::INVALID);
      return;
    }

    if (q.isFalse()) {
      ++num_trivial;
      continue;
    }

    // TODO: benchmark: reset() or new solver every time?
    Solver s;
    s.add(q);
    auto res = s.check();
    if (!res.isUnsat()) {
      error(res);
      return;
    }
  }
}

void solver_print_stats(ostream &os) {
  float total = num_queries / 100.0;
  float trivial_pc = num_queries == 0 ? 0 :
                       (num_trivial * 100.0) / (num_trivial + num_queries);
  float unknown_pc = num_queries == 0 ? 0 : num_unknown / total;
  float sat_pc     = num_queries == 0 ? 0 : num_sats / total;
  float unsat_pc   = num_queries == 0 ? 0 : num_unsats / total;

  os << fixed << setprecision(1);
  os << "\n------------------- SMT STATS -------------------\n"
        "Num queries: " << num_queries << "\n"
        "Num invalid: " << num_invalid << "\n"
        "Num skips:   " << num_skips << "\n"
        "Num trivial: " << num_trivial << " (" << trivial_pc << "%)\n"
        "Num unknown: " << num_unknown << " (" << unknown_pc << "%)\n"
        "Num SAT:     " << num_sats << " (" << sat_pc << "%)\n"
        "Num UNSAT:   " << num_unsats << " (" << unsat_pc << "%)\n";
}


EnableSMTQueriesTMP::EnableSMTQueriesTMP() : old(config::skip_smt) {
  config::skip_smt = false;
}

EnableSMTQueriesTMP::~EnableSMTQueriesTMP() {
  config::skip_smt = old;
}


void solver_init() {
  tactic = {
    "simplify",
    "propagate-values",
    "simplify",
    "smt"
  };
}

void solver_destroy() {
  tactic = Tactic();
}

}
