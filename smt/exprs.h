#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/expr.h"
#include "util/compiler.h"
#include "util/spaceship.h"
#include <cassert>
#include <compare>
#include <map>
#include <optional>
#include <ostream>
#include <set>
#include <tuple>
#include <utility>

namespace smt {

class AndExpr {
  std::set<expr> exprs;

public:
  AndExpr() = default;
  template <typename T>
  AndExpr(T &&e) { add(std::forward<T>(e)); }

  void add(const expr &e, unsigned limit = 16);
  void add(expr &&e, unsigned limit = 16);
  void add(const AndExpr &other);
  void del(const AndExpr &other);
  void reset();
  bool contains(const expr &e) const;
  expr operator()() const;
  operator bool() const;
  bool isTrue() const { return exprs.empty(); }
  friend std::ostream &operator<<(std::ostream &os, const AndExpr &e);
};


class OrExpr {
  std::set<expr> exprs;

public:
  void add(const expr &e);
  void add(expr &&e);
  void add(const OrExpr &other);
  expr operator()() const;
  bool empty() const { return exprs.empty(); }
  friend std::ostream &operator<<(std::ostream &os, const OrExpr &e);
};


template <typename T>
class DisjointExpr {
  std::map<T, expr> vals; // val -> domain
  std::optional<T> default_val;

public:
  DisjointExpr() = default;
  DisjointExpr(const T &default_val) : default_val(default_val) {}
  DisjointExpr(const std::optional<T> &default_val) : default_val(default_val){}
  DisjointExpr(T &&default_val) : default_val(std::move(default_val)) {}
  DisjointExpr(const expr &e, unsigned depth_limit);

  template <typename V, typename D>
  void add(V &&val, D &&domain) {
    if (domain.isFalse())
      return;
    if (domain.isTrue())
      vals.clear();

    auto [I, inserted] = vals.try_emplace(std::forward<V>(val),
                                          std::forward<D>(domain));
    if (!inserted)
      I->second |= std::forward<D>(domain);
  }

  void add_disj(const DisjointExpr<T> &other, const expr &domain) {
    assert(!default_val && !other.default_val);
    for (auto &[v, d] : other.vals) {
      add(v, d && domain);
    }
  }

  void add_disj(DisjointExpr<T> &&other, const expr &domain) {
    assert(!default_val && !other.default_val);
    for (auto &[v, d] : other.vals) {
      add(std::move(const_cast<T&>(v)), d && domain);
    }
  }

  std::optional<T> operator()() const& {
    std::optional<T> ret;
    for (auto &[val, domain] : vals) {
      if (domain.isTrue())
        return val;

      ret = ret ? T::mkIf(domain, val, *std::move(ret)) : val;
    }
    if (ret)
      return ret;
    return default_val;
  }

  // argument is the value used if the domain is false
  // not the same as default, which is used only if no element is present
  std::optional<T> mk(std::optional<T> ret) && {
    for (auto &[val0, domain] : vals) {
      auto &val = const_cast<T&>(val0);
      if (domain.isTrue())
        return std::move(val);

      ret = ret ? T::mkIf(domain, std::move(val), *std::move(ret))
                : std::move(val);
    }
    if (ret)
      return ret;
    return std::move(default_val);
  }

  std::optional<T> operator()() && { return std::move(*this).mk({}); }

  expr domain() const {
    OrExpr ret;
    for (auto &[val, domain] : vals) {
      ret.add(domain);
    }
    return std::move(ret)();
  }

  std::optional<T> lookup(const expr &domain) const {
    for (auto &[v, d] : vals) {
      if (d.eq(domain))
        return v;
    }
    return {};
  }

  auto begin() const { return vals.begin(); }
  auto end() const   { return vals.end(); }
  auto size() const  { return vals.size(); }
  bool empty() const { return vals.empty() && !default_val; }
};


// non-deterministic choice of one of the options with potentially overlapping
// domains
template <typename T>
class ChoiceExpr {
  std::map<T, expr> vals; // val -> domain

public:
  template <typename V, typename D>
  void add(V &&val, D &&domain) {
    if (domain.isFalse())
      return;
    auto [I, inserted] = vals.try_emplace(std::forward<V>(val),
                                          std::forward<D>(domain));
    if (!inserted)
      I->second |= std::forward<D>(domain);
  }

  operator bool() const {
    return !vals.empty();
  }

  expr domain() const {
    OrExpr ret;
    for (auto &p : vals) {
      ret.add(expr(p.second));
    }
    return ret();
  }

  // returns: data, domain, quant var, precondition
  std::tuple<T,expr,expr,expr> operator()() && {
    if (vals.size() == 1)
      return { vals.begin()->first, vals.begin()->second, expr(), true };

    expr dom = domain();
    unsigned bits = util::ilog2_ceil(vals.size() + !dom.isTrue(), false);
    expr qvar = expr::mkFreshVar("choice", expr::mkUInt(0, bits));

    T ret;
    expr pre = !dom;
    auto I = vals.begin();
    bool first = true;

    for (unsigned i = vals.size(); i > 0; --i) {
      auto cmp = qvar == (i-1);
      pre = expr::mkIf(cmp, I->second, pre);
      ret = first ? std::move(I->first)
                  : T::mkIf(cmp, std::move(I->first), std::move(ret));
      first = false;
      ++I;
    }

    return { std::move(ret), std::move(dom), std::move(qvar), std::move(pre) };
  }
};


class FunctionExpr {
  std::map<expr, expr> fn; // key -> val

public:
  FunctionExpr() = default;
  void add(const expr &key, expr &&val);
  void add(const FunctionExpr &other);

  std::optional<expr> operator()(const expr &key) const;
  const expr* lookup(const expr &key) const;

  FunctionExpr simplify() const;

  auto begin() const { return fn.begin(); }
  auto end() const { return fn.end(); }
  bool empty() const { return fn.empty(); }

  auto operator<=>(const FunctionExpr &rhs) const = default;

  friend std::ostream& operator<<(std::ostream &os, const FunctionExpr &e);
};

}

static inline smt::expr merge(std::pair<smt::AndExpr, smt::expr> e) {
  e.first.add(std::move(e.second));
  return std::move(e.first)();
}
