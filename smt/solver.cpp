// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/solver.h"
#include "smt/ctx.h"
#include "util/compiler.h"
#include "util/config.h"
#include "util/file.h"
#include <cassert>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>
#include <z3.h>

using namespace smt;
using namespace util;
using namespace std;
using util::config::dbg;

static bool tactic_verbose = false;

static unsigned num_queries = 0;
static unsigned num_skips = 0;
static unsigned num_invalid = 0;
static unsigned num_trivial = 0;
static unsigned num_sats = 0;
static unsigned num_unsats = 0;
static unsigned num_timeout = 0;
static unsigned num_errors = 0;

namespace {

struct Goal {
  Z3_goal goal = nullptr;

  Goal() {
    if (tactic_verbose) {
      goal = Z3_mk_goal(ctx(), true, false, false);
      Z3_goal_inc_ref(ctx(), goal);
    }
  }

  Goal(const Goal &g) noexcept : goal(g.goal) {
    if (goal)
      Z3_goal_inc_ref(ctx(), goal);
  }

  Goal(Goal &&g) noexcept {
    swap(goal, g.goal);
  }

  void operator=(Goal &&g) noexcept {
    swap(goal, g.goal);
  }

  ~Goal() {
    if (goal)
      Z3_goal_dec_ref(ctx(), goal);
  }

  void add(Z3_ast ast) {
    if (tactic_verbose)
      Z3_goal_assert(ctx(), goal, ast);
  }

  void reset() {
    if (tactic_verbose)
      Z3_goal_reset(ctx(), goal);
  }

  bool isDecided() const {
    return Z3_goal_is_decided_sat(ctx(), goal) ||
           Z3_goal_is_decided_unsat(ctx(), goal);
  }

  const char* toString() const {
    return Z3_goal_to_string(ctx(), goal);
  }
};


struct TacticResult {
  Z3_apply_result r = nullptr;

  TacticResult(Z3_apply_result r = nullptr) : r(r) {
    if (r)
      Z3_apply_result_inc_ref(ctx(), r);
  }

  TacticResult(TacticResult &&other) noexcept {
    swap(r, other.r);
  }

  ~TacticResult() {
    if (r)
      Z3_apply_result_dec_ref(ctx(), r);
  }

  void operator=(TacticResult &&other) {
    swap(r, other.r);
  }

  Goal toGoal() const {
    Goal goal;
    for (unsigned i = 0, e = Z3_apply_result_get_num_subgoals(ctx(), r);
          i != e; ++i) {
      auto ng = Z3_apply_result_get_subgoal(ctx(), r, i);
      for (unsigned ii = 0, ee = Z3_goal_size(ctx(), ng); ii != ee; ++ii) {
        Z3_goal_assert(ctx(), goal.goal, Z3_goal_formula(ctx(), ng, ii));
      }
    }
    return goal;
  }
};


struct Probe {
  Z3_probe p = nullptr;
  const char *name;

  Probe(const char *name) : p(Z3_mk_probe(ctx(), name)), name(name) {
    Z3_probe_inc_ref(ctx(), p);
  }

  Probe(Probe &&other) noexcept {
    swap(p, other.p);
    name = other.name;
  }

  void operator=(Probe &&other) noexcept {
    swap(p, other.p);
    name = other.name;
  }

  ~Probe() {
    if (p)
      Z3_probe_dec_ref(ctx(), p);
  }

  bool check(const Goal &goal) const {
    return Z3_probe_apply(ctx(), p, goal.goal) != 0.0;
  }
};


struct Tactic {
  Z3_tactic t = nullptr;

  Tactic(Z3_tactic t = nullptr) : t(t) {
    if (t)
      Z3_tactic_inc_ref(ctx(), t);
  }

  Tactic(Tactic &&other) noexcept {
    swap(t, other.t);
  }

  void destroy() {
    if (t)
      Z3_tactic_dec_ref(ctx(), t);
  }

  virtual ~Tactic() { destroy(); }

  void set(Z3_tactic newt) {
    Z3_tactic_inc_ref(ctx(), newt);
    destroy();
    t = newt;
  }

  virtual TacticResult exec(const Goal &goal) const {
    UNREACHABLE();
    return {};
  };
};


struct NamedTactic final : public Tactic {
  const char *name = nullptr;

  NamedTactic(const char *name) : Tactic(Z3_mk_tactic(ctx(), name)) {
    this->name = name;
  }

  TacticResult exec(const Goal &goal) const override {
    dbg() << "\nApplying " << name << endl;
    Tactic to(Z3_tactic_or_else(ctx(),
                                Tactic(Z3_tactic_try_for(ctx(), t, 10000)).t,
                                Tactic(Z3_tactic_skip(ctx())).t));
    return Z3_tactic_apply(ctx(), to.t, goal.goal);
  }
};


struct IfTactic final : public Tactic {
  Probe p;
  unique_ptr<Tactic> then, els;

  IfTactic(Probe &&p, unique_ptr<Tactic> &&then, unique_ptr<Tactic> &&els)
    : Tactic(Z3_tactic_cond(ctx(), p.p, then->t, els->t)),
      p(std::move(p)), then(std::move(then)), els(std::move(els)) {}

  TacticResult exec(const Goal &goal) const override {
    bool probe = p.check(goal);
    dbg() << "\nProbe " << p.name << (probe ? " is true\n" : " is false\n");
    return (probe ? then : els)->exec(goal);
  }
};


class AndTactic final : public Tactic {
  vector<unique_ptr<Tactic>> tactics;

  void append(Z3_tactic t) {
    set(this->t ? Z3_tactic_and_then(ctx(), this->t, t) : t);
  }

public:
  AndTactic(initializer_list<const char*> ts) {
    for (auto *name : ts) {
      NamedTactic t(name);
      append(t.t);
      if (tactic_verbose)
        tactics.emplace_back(make_unique<NamedTactic>(std::move(t)));
    }
  }

  template <typename T1, typename T2>
  void appendIf(const char *probe, T1 &&then, T2 &&els) {
    IfTactic t(probe, make_unique<T1>(std::move(then)),
               make_unique<T2>(std::move(els)));
    append(t.t);
    if (tactic_verbose)
      tactics.emplace_back(make_unique<IfTactic>(std::move(t)));
  }

  TacticResult exec(const Goal &goal0) const override {
    Goal goal(goal0);
    TacticResult r;
    string last_result;

    for (auto &t : tactics) {
      r = t->exec(goal);
      auto newgoal = r.toGoal();

      auto new_r = newgoal.toString();
      if (new_r != last_result) {
        dbg() << new_r << '\n';
        last_result = new_r;
      } else {
        dbg() << "(no change)\n";
      }
      goal = std::move(newgoal);
      if (goal.isDecided())
        break;
    }
    return r;
  }
};


class TopLevelTactic {
  AndTactic tactics;
  Goal goal;

public:
  TopLevelTactic(initializer_list<const char*> ts) : tactics(std::move(ts)) {}

  template <typename T1, typename T2>
  void appendIf(const char *probe, T1 &&then, T2 &&els) {
    tactics.appendIf(probe, std::move(then), std::move(els));
  }

  void add(Z3_ast ast) { goal.add(ast); }
  void reset_solver()  { goal.reset(); }

  void check() {
    if (tactic_verbose)
      goal = tactics.exec(goal).toGoal();
  }

  Z3_solver getSolver() const {
    return Z3_mk_solver_from_tactic(ctx(), tactics.t);
  }

  friend class smt::Solver;
};
}

static optional<TopLevelTactic> tactic;


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

expr Model::eval(const expr &var, bool complete) const {
  Z3_ast val;
  ENSURE(Z3_model_eval(ctx(), m, var(), complete, &val));
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

bool Model::hasFnModel(const expr &fn) const {
  auto fn_decl = fn.decl();
  return fn_decl ? Z3_model_has_interp(ctx(), m, fn_decl) : false;
}

pair<expr, expr> Model::iterator::operator*(void) const {
  auto decl = Z3_model_get_const_decl(ctx(), m, idx);
  return { expr::mkConst(decl), Z3_model_get_const_interp(ctx(), m, decl) };
}

Model::iterator Model::end() const {
  return { nullptr, Z3_model_get_num_consts(ctx(), m) };
}

pair<string, FnModel> Model::fns::iterator::operator*(void) const {
  auto decl = Z3_model_get_func_decl(ctx(), m, idx);
  auto name = Z3_get_symbol_string(ctx(), Z3_get_decl_name(ctx(), decl));
  FnModel model(Z3_model_get_func_interp(ctx(), m, decl), name);
  return { std::move(name), std::move(model) };
}

Model::fns::iterator Model::fns::end() const {
  return { nullptr, Z3_model_get_num_funcs(ctx(), m) };
}

ostream& operator<<(ostream &os, const Model &m) {
  return os << Z3_model_to_string(ctx(), m.m);
}


FnModel::FnModel(Z3_func_interp f, const string &fn_name)
  : f(f), fn_name(fn_name) {
  Z3_func_interp_inc_ref(ctx(), f);
}

FnModel::~FnModel() {
  if (f)
    Z3_func_interp_dec_ref(ctx(), f);
}

FnModel::iterator FnModel::end() const {
 return { nullptr, nullptr, Z3_func_interp_get_num_entries(ctx(), f) };
}

pair<expr, expr> FnModel::iterator::operator*(void) const {
  auto entry = Z3_func_interp_get_entry(ctx(), f, idx);
  Z3_func_entry_inc_ref(ctx(), entry);
  expr value = Z3_func_entry_get_value(ctx(), entry);

  vector<expr> args;
  for (unsigned i = 0, e = Z3_func_entry_get_num_args(ctx(), entry);
       i != e; ++i) {
    args.emplace_back(expr(Z3_func_entry_get_arg(ctx(), entry, i)));
  }
  Z3_func_entry_dec_ref(ctx(), entry);

  auto var = expr::mkUF(fn_name, args, value);
  return { std::move(var), std::move(value) };
}

expr FnModel::getElse() const {
  auto ast = Z3_func_interp_get_else(ctx(), f);
  return ast ? expr(ast) : expr();
}


static bool print_queries = false;
void solver_print_queries(bool yes) {
  print_queries = yes;
}

void solver_tactic_verbose(bool yes) {
  tactic_verbose = yes;
}

Solver::Solver(bool simple) {
  s = simple ? Z3_mk_simple_solver(ctx())
             : tactic->getSolver();
  Z3_solver_inc_ref(ctx(), s);
}

Solver::~Solver() {
  Z3_solver_dec_ref(ctx(), s);
  tactic->reset_solver();
}

void Solver::add(const expr &e) {
  if (e.isFalse()) {
    is_unsat = true;
  } else if (e.isValid()) {
    auto ast = e();
    Z3_solver_assert(ctx(), s, ast);
    tactic->add(ast);
  } else {
    valid = false;
  }
}

void Solver::block(const Model &m, Solver *sneg) {
  set<expr> assignments;
  for (const auto &[var, val] : m) {
    assignments.insert(var == val);
  }

  if (sneg) {
    // simple left-to-right variable discard algorithm
    for (auto I = assignments.begin(); I != assignments.end(); ) {
      SolverPush push(*sneg);
      expr val = *I;
      I = assignments.erase(I);

      sneg->add(expr::mk_and(assignments));
      if (!sneg->check().isUnsat())
        assignments.insert(std::move(val));
    }
  }

  add(!expr::mk_and(assignments));
}

void Solver::reset() {
  Z3_solver_reset(ctx(), s);
  tactic->reset_solver();
}

expr Solver::assertions() const {
  auto vect = Z3_solver_get_assertions(ctx(), s);
  Z3_ast_vector_inc_ref(ctx(), vect);
  expr ret(true);

  for (unsigned i = 0, e = Z3_ast_vector_size(ctx(), vect); i != e; ++i) {
    ret &= Z3_ast_vector_get(ctx(), vect, i);
  }

  Z3_ast_vector_dec_ref(ctx(), vect);
  return ret;
}

Result Solver::check(const char *query_name) const {
  if (!valid) {
    ++num_invalid;
    return Result::INVALID;
  }

  if (is_unsat) {
    ++num_trivial;
    return Result::UNSAT;
  }

  if (!config::smt_benchmark_dir.empty()) {
    const char *banner =
    R"(Alive2 compiler optimization refinement query
; More info in "Alive2: Bounded Translation Validation for LLVM", PLDI'21.)";
    expr fml = assertions();
    if (!fml.isTrue()) {
      auto str = Z3_benchmark_to_smtlib_string(ctx(), banner, nullptr, nullptr,
                                               nullptr, 0, nullptr, fml());
      ofstream file(get_random_filename(config::smt_benchmark_dir, "smt2", query_name));
      if (!file.is_open()) {
        dbg() << "Alive2: Couldn't open smtlib benchmark file!" << endl;
        exit(1);
      }
      file << str;
    }
  }

  if (config::skip_smt) {
    ++num_skips;
    return Result::SKIP;
  }

  ++num_queries;
  if (print_queries) {
    dbg() << "\nSMT query";
    if (query_name != nullptr) {
      dbg() << " (" << query_name << ")";
    }
    dbg() << ":\n" << Z3_solver_to_string(ctx(), s) << endl;
  }

  tactic->check();

  switch (Z3_solver_check(ctx(), s)) {
  case Z3_L_FALSE:
    ++num_unsats;
    return Result::UNSAT;
  case Z3_L_TRUE:
    ++num_sats;
    return Z3_solver_get_model(ctx(), s);
  case Z3_L_UNDEF: {
    string_view reason = Z3_solver_get_reason_unknown(ctx(), s);
    if (reason == "timeout") {
      ++num_timeout;
      return Result::TIMEOUT;
    }
    ++num_errors;
    return { Result::ERROR, string(reason) };
  }
  default:
    UNREACHABLE();
  }
}

Result check_expr(const expr &e, const char *query_name) {
  Solver s;
  s.add(e);
  return s.check(query_name);
}


SolverPush::SolverPush(Solver &s) : s(s), valid(s.valid), is_unsat(s.is_unsat) {
  Z3_solver_push(ctx(), s.s);
}

SolverPush::~SolverPush() {
  s.valid = valid;
  s.is_unsat = is_unsat;
  Z3_solver_pop(ctx(), s.s, 1);
}


void solver_print_stats(ostream &os) {
  float total = num_queries / 100.0;
  float trivial_pc = num_queries == 0 ? 0 :
                       (num_trivial * 100.0) / (num_trivial + num_queries);
  float to_pc      = num_queries == 0 ? 0 : num_timeout / total;
  float error_pc   = num_queries == 0 ? 0 : num_errors / total;
  float sat_pc     = num_queries == 0 ? 0 : num_sats / total;
  float unsat_pc   = num_queries == 0 ? 0 : num_unsats / total;

  os << fixed << setprecision(1);
  os << "\n------------------- SMT STATS -------------------\n"
        "Num queries: " << num_queries << "\n"
        "Num invalid: " << num_invalid << "\n"
        "Num skips:   " << num_skips << "\n"
        "Num trivial: " << num_trivial << " (" << trivial_pc << "%)\n"
        "Num timeout: " << num_timeout << " (" << to_pc << "%)\n"
        "Num errors:  " << num_errors << " (" << error_pc << "%)\n"
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
  tactic.emplace({
    "simplify",
    "propagate-values",
    "simplify",
    "elim-uncnstr",
    "qe-light",
    "simplify",
    "elim-uncnstr",
    "reduce-args",
    "qe-light",
    "simplify",
    "smt"
  });
#if 0
  tactic->appendIf("is-qfbv",
                   AndTactic({
                    "bit-blast", "simplify", "solve-eqs", "aig", "sat"}),
                   NamedTactic("smt"));
#endif
}

void solver_destroy() {
  tactic.reset();
}

}
