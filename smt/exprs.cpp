// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/exprs.h"
#include "smt/smt.h"
#include "util/compiler.h"
#include "util/spaceship.h"
#include <vector>

using namespace std;

namespace smt {

void AndExpr::add(const expr &e, unsigned limit) {
  if (e.isTrue())
    return;

  expr a, b;
  if (limit > 0 && e.isAnd(a, b)) {
    add(std::move(a), limit-1);
    add(std::move(b), limit-1);
    return;
  }
  exprs.insert(e);
}

void AndExpr::add(expr &&e, unsigned limit) {
  if (e.isTrue())
    return;

  expr a, b;
  if (limit > 0 && e.isAnd(a, b)) {
    add(std::move(a), limit-1);
    add(std::move(b), limit-1);
    return;
  }
  exprs.insert(std::move(e));
}

void AndExpr::add(const AndExpr &other) {
  exprs.insert(other.exprs.begin(), other.exprs.end());
}

void AndExpr::del(const AndExpr &other) {
  for (auto &e : other.exprs)
    exprs.erase(e);
}

void AndExpr::reset() {
  exprs.clear();
}

bool AndExpr::contains(const expr &e) const {
  return exprs.count(e);
}

expr AndExpr::operator()() const {
  return expr::mk_and(exprs);
}

AndExpr::operator bool() const {
  return !exprs.count(false);
}

ostream &operator<<(ostream &os, const AndExpr &e) {
  return os << e();
}


void OrExpr::add(expr &&e) {
  if (!e.isFalse())
    exprs.insert(std::move(e));
}

void OrExpr::add(const OrExpr &other) {
  exprs.insert(other.exprs.begin(), other.exprs.end());
}

expr OrExpr::operator()() const {
  return expr::mk_or(exprs);
}

ostream &operator<<(ostream &os, const OrExpr &e) {
  return os << e();
}


// do simple rewrites that cover the common cases
static expr simplify(const expr &e, const expr &cond, bool negate) {
  // (bvadd ((_ extract ..) (ite cond X Y)) Z)
  // -> (bvadd ((_ extract ..) X) Z)  or  (bvadd ((_ extract ..) Y) Z)
  // NB: although this potentially increases the circuit size, it allows further
  // simplifications down the road, and has been shown to be benefitial to perf
  expr a, b;
  if (e.isAdd(a, b)) {
    auto test = [&](const expr &a, const expr &b) -> expr {
      expr extract, ifcond, t, e;
      unsigned high, low;
      if (b.isExtract(extract, high, low) &&
          extract.isIf(ifcond, t, e) &&
          ifcond.eq(cond)) {
        return a + (negate ? e : t).extract(high, low);
      }
      return {};
    };
    if (auto r = test(a, b); r.isValid()) return r;
    if (auto r = test(b, a); r.isValid()) return r;
  }
  return e;
}

template<>
DisjointExpr<expr>::DisjointExpr(const expr &e, unsigned depth_limit) {
  if (depth_limit-- == 0) {
    add(e, expr(true));
    return;
  }

  vector<pair<expr, expr>> worklist = { {e, true} };
  expr cond, then, els, a, b;
  unsigned high, low;

  do {
    // Limit exponential growth
    if ((worklist.size() + vals.size()) >= 32 ||
        hit_half_memory_limit()) {
      for (auto &[v, c] : worklist) {
        add(std::move(v), std::move(c));
      }
      break;
    }

    auto [v, c] = worklist.back();
    worklist.pop_back();

    if (v.isIf(cond, then, els)) {
      worklist.emplace_back(std::move(then), c && cond);
      worklist.emplace_back(std::move(els), c && !cond);
    }
    else if (v.isConcat(a, b)) {
      DisjointExpr<expr> lhs(a, depth_limit);
      DisjointExpr<expr> rhs(b, depth_limit);
      if (lhs.size() == 1 && rhs.size() == 1) {
        add(std::move(v), std::move(c));
        continue;
      }

      for (auto &[lhs_v, lhs_domain] : lhs) {
        if (auto rhs_val = rhs.lookup(lhs_domain)) {
          add(lhs_v.concat(*rhs_val), c && lhs_domain);
        } else {
          expr from;
          bool negate = true;
          if (!lhs_domain.isNot(from)) {
            from = lhs_domain;
            negate = false;
          }

          for (auto &[rhs_v, rhs_domain] : rhs) {
            add(lhs_v.concat(simplify(rhs_v, from, negate)),
                c && lhs_domain && rhs_domain);
          }
        }
      }
    }
    else if (v.isExtract(a, high, low)) {
      DisjointExpr<expr> vals(a, depth_limit);
      if (vals.size() == 1) {
        add(std::move(v), std::move(c));
        continue;
      }

      for (auto &[v, domain] : vals) {
        add(v.extract(high, low), c && domain);
      }
    }
    else {
      add(std::move(v), std::move(c));
    }
  } while (!worklist.empty());
}


void FunctionExpr::add(const expr &key, expr &&val) {
  ENSURE(fn.emplace(key, std::move(val)).second);
}

void FunctionExpr::add(const FunctionExpr &other) {
  fn.insert(other.fn.begin(), other.fn.end());
}

void FunctionExpr::del(const expr &key) {
  fn.erase(key);
}

optional<expr> FunctionExpr::operator()(const expr &key) const {
  DisjointExpr disj(default_val);
  for (auto &[k, v] : fn) {
    disj.add(v, k == key);
  }
  return std::move(disj)();
}

const expr* FunctionExpr::lookup(const expr &key) const {
  auto I = fn.find(key);
  return I != fn.end() ? &I->second : nullptr;
}

FunctionExpr FunctionExpr::simplify() const {
  FunctionExpr newfn;
  if (default_val)
    newfn.default_val = default_val->simplify();

  for (auto &[k, v] : fn) {
    newfn.add(k.simplify(), v.simplify());
  }
  return newfn;
}

weak_ordering FunctionExpr::operator<=>(const FunctionExpr &rhs) const {
  if (auto cmp = fn <=> rhs.fn;
      is_neq(cmp))
    return cmp;

  // libstdc++'s optional::operator<=> is buggy
  // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=98842
  if (default_val && rhs.default_val)
    return *default_val <=> *rhs.default_val;

  return (bool)default_val <=> (bool)rhs.default_val;
}

ostream& operator<<(ostream &os, const FunctionExpr &f) {
  os << "{\n";
  for (auto &[k, v] : f) {
    os << k << ": " << v << '\n';
  }
  if (f.default_val)
    os << "default: " << *f.default_val << '\n';
  return os << '}';
}

}
