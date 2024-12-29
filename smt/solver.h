#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/expr.h"
#include <cassert>
#include <ostream>
#include <string>
#include <utility>

typedef struct _Z3_func_interp* Z3_func_interp;
typedef struct _Z3_model* Z3_model;
typedef struct _Z3_solver* Z3_solver;

namespace smt {

class FnModel;

class Model {
  Z3_model m = nullptr;

  Model() = default;
  Model(Z3_model m);
  ~Model();

  friend class Result;

public:
  Model(Model &&other) noexcept {
    std::swap(other.m, m);
  }

  void operator=(Model &&other);

  expr operator[](const expr &var) const { return eval(var, true); }
  expr eval(const expr &var, bool complete = false) const;
  uint64_t getUInt(const expr &var) const;
  int64_t getInt(const expr &var) const;
  bool hasFnModel(const expr &fn) const;

  class iterator {
    Z3_model m;
    unsigned idx;
    iterator(Z3_model m, unsigned idx) : m(m), idx(idx) {}
  public:
    void operator++(void) { ++idx; }
    std::pair<expr, expr> operator*(void) const; // <var, value>
    bool operator!=(const iterator &rhs) const { return idx != rhs.idx; }
    friend class Model;
  };

  // WARNING: the parent Model class has to be alive while iterators are in use.
  iterator begin() const { return { m, 0 }; }
  iterator end() const;

  class fns {
    Z3_model m;
    fns(Z3_model m) : m(m) {}
  public:
    class iterator {
      Z3_model m;
      unsigned idx;
      iterator(Z3_model m, unsigned idx) : m(m), idx(idx) {}
    public:
      void operator++(void) { ++idx; }
      std::pair<std::string, FnModel> operator*(void) const;
      bool operator!=(const iterator &rhs) const { return idx != rhs.idx; }
      friend class fns;
    };
    iterator begin() const { return { m, 0 }; }
    iterator end() const;
    friend class Model;
  };

  fns getFns() const { return m; }

  friend std::ostream& operator<<(std::ostream &os, const Model &m);
};


class FnModel {
  Z3_func_interp f;
  std::string fn_name;
  FnModel(Z3_func_interp f, const std::string &fn_name);

public:
  FnModel(FnModel &&other) noexcept : f(0) {
    std::swap(other.f, f);
    std::swap(other.fn_name, fn_name);
  }
  ~FnModel();

  class iterator {
    Z3_func_interp f;
    const char *fn_name;
    unsigned idx;
    iterator(Z3_func_interp f, const char *fn_name, unsigned idx)
      : f(f), fn_name(fn_name), idx(idx) {}
  public:
    void operator++(void) { ++idx; }
    std::pair<expr, expr> operator*(void) const; // <fn, value>
    bool operator!=(const iterator &rhs) const { return idx != rhs.idx; }
    friend class FnModel;
  };

  iterator begin() const { return { f, fn_name.c_str(), 0 }; }
  iterator end() const;

  expr getElse() const;

  friend class Model::fns::iterator;
};


class Result {
public:
  enum answer { UNSAT, SAT, INVALID, SKIP, TIMEOUT, ERROR };

  Result() : a(ERROR) {}

  bool isSat() const { return a == SAT; }
  bool isUnsat() const { return a == UNSAT; }
  bool isInvalid() const { return a == INVALID; }
  bool isSkip() const { return a == SKIP; }
  bool isTimeout() const { return a == TIMEOUT; }
  bool isError() const { return a == ERROR; }

  auto& getReason() const { return reason; }

  const Model& getModel() const {
    assert(isSat());
    return m;
  }

private:
  Model m;
  answer a;
  std::string reason;

  Result(answer a) : a(a) {}
  Result(answer a, std::string &&reason) : a(a), reason(std::move(reason)) {}
  Result(Z3_model m) : m(m), a(SAT) {}

  friend class Solver;
};


class Solver {
  Z3_solver s;
  bool valid = true;
  bool is_unsat = false;

public:
  Solver(bool simple = false);
  ~Solver();

  void add(const expr &e);
  // use a negated solver for minimization
  void block(const Model &m, Solver *sneg = nullptr);
  void reset();

  expr assertions() const;

  Result check(const char *query_name = nullptr, bool dont_skip = false) const;

  friend class SolverPush;
};

Result check_expr(const expr &e, const char *query_name = nullptr);


class SolverPush {
  Solver &s;
  bool valid, is_unsat;
public:
  SolverPush(Solver &s);
  ~SolverPush();
};


void solver_print_queries(bool yes);
void solver_tactic_verbose(bool yes);
void solver_print_stats(std::ostream &os);


struct EnableSMTQueriesTMP {
  bool old;
  EnableSMTQueriesTMP();
  ~EnableSMTQueriesTMP();
};


void solver_init();
void solver_destroy();

}
