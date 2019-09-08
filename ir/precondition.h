#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/constant.h"
#include "ir/value.h"
#include <string>
#include <string_view>
#include <vector>

namespace smt { class Model; }

namespace IR {

class Predicate {
public:
  virtual void print(std::ostream &os) const = 0;
  virtual smt::expr toSMT(State &s) const = 0;
  virtual smt::expr getTypeConstraints() const;
  virtual void fixupTypes(const smt::Model &m);
  virtual ~Predicate() {}
};


class BoolPred final : public Predicate {
public:
  enum Pred { AND, OR };

private:
  Predicate &lhs, &rhs;
  Pred pred;

public:
  BoolPred(Predicate &lhs, Predicate &rhs, Pred pred)
    : lhs(lhs), rhs(rhs), pred(pred) {}
  void print(std::ostream &os) const override;
  smt::expr toSMT(State &s) const override;
};


class FnPred final : public Predicate {
  enum Fn { AddNSW, AddNUW, SubNSW, SubNUW, MulNSW, MulNUW, ShlNSW, ShlNUW } fn;
  std::vector<Value*> args;

  smt::expr mkMustAnalysis(State &s, smt::expr &&e) const;

public:
  FnPred(std::string_view name, std::vector<Value*> &&args);
  void print(std::ostream &os) const override;
  smt::expr toSMT(State &s) const override;
  smt::expr getTypeConstraints() const override;
  void fixupTypes(const smt::Model &m) override;
};

struct FnPredException {
  std::string str;
  FnPredException(std::string &&str) : str(std::move(str)) {}
};


class CmpPred final : public Predicate {
public:
  enum Pred { EQ, NE, SLE, SLT, SGE, SGT, ULE, ULT, UGE, UGT };

private:
  Constant &lhs, &rhs;
  Pred pred;

public:
  CmpPred(Constant &lhs, Constant &rhs, Pred pred)
    : lhs(lhs), rhs(rhs), pred(pred) {}

  void print(std::ostream &os) const override;
  smt::expr toSMT(State &s) const override;
  smt::expr getTypeConstraints() const override;
  void fixupTypes(const smt::Model &m) override;
};

}
