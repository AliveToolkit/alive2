// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/fast_math.h"
#include "ir/attrs.h"
#include "util/spaceship.h"
#include "util/unionfind.h"
#include <map>
#include <vector>

#define DEBUG_FMF

#ifdef DEBUG_FMF
# include <iostream>
#endif

using namespace IR;
using namespace smt;
using namespace util;
using namespace std;

// Home-made e-graph to represent all possible fast-math reassociations

namespace {

class EGraph;

struct Node {
  enum { Add, Sub, Mul, Div, Neg, Leaf } operation;
  unsigned op1 = -1u, op2 = -1u;
  expr leaf;
  expr rounding;
  unsigned flags = 0;

  auto operator<=>(const Node &other) const = default;

#ifdef DEBUG_FMF
  friend ostream& operator<<(ostream &os, const Node &n) {
    const char *op = nullptr;
    switch (n.operation) {
      case Add:  op = "add "; break;
      case Sub:  op = "sub "; break;
      case Mul:  op = "mul "; break;
      case Div:  op = "div "; break;
      case Neg:  op = "neg "; break;
      case Leaf: op = "leaf "; break;
    }
    os << op;

    if (n.operation == Leaf)
      return os << n.leaf;

    os << n.op1;
    if (n.op2 != -1u) {
      os << ", " << n.op2;
    }

    FastMathFlags fmf{n.flags};
    if (!fmf.isNone())
      os << "\t# " << fmf;
    return os;
  }
#endif
};


class EGraph {
  UnionFind uf;                          // id -> root
  map<Node, unsigned> nodes;             // node -> id
  vector<vector<const Node*>> nodes_eqs; // root -> equiv nodes

  unsigned get(Node &&n) {
    auto [I, inserted] = nodes.try_emplace(std::move(n), 0);
    if (inserted) {
      I->second = uf.mk();
      nodes_eqs.emplace_back(1, &I->first);
    }
    return I->second;
  }

  void decl_equivalent(Node &&node, unsigned n1) {
    unsigned n2 = get(std::move(node));
    unsigned new_root = uf.merge(n2, n1);
    auto &root = nodes_eqs[new_root];

    auto merge = [&](unsigned n) {
      if (n != new_root) {
        auto &prev = nodes_eqs[n];
        root.insert(root.end(), prev.begin(), prev.end());
        prev = vector<const Node*>();
      }
    };
    merge(n1);
    merge(n2);
  }

  void remove(const Node &node) {
    auto I = nodes.find(node);
    assert(I != nodes.end());
    erase(nodes_eqs[I->second], &node);
    nodes.erase(I);
  }

public:
  unsigned root(unsigned n) {
    return uf.find(n);
  }

  unsigned get(const expr &e0) {
    Node n;
    expr rounding, a, b;
    bool is_leaf = false;

    expr e = e0;

    while (true) {
      if (e.isFPAdd(rounding, a, b)) {
        n.op2 = get(b);
        n.operation = Node::Add;
      } else if (e.isFPSub(rounding, a, b)) {
        n.op2 = get(b);
        n.operation = Node::Sub;
      } else if (e.isFPMul(rounding, a, b)) {
        n.op2 = get(b);
        n.operation = Node::Mul;
      } else if (e.isFPDiv(rounding, a, b)) {
        n.op2 = get(b);
        n.operation = Node::Div;
      } else if (e.isFPNeg(a)) {
        n.operation = Node::Neg;
      } else if (auto name = e.fn_name(); !name.empty()) {
        if (name == "reassoc") {
          n.flags |= FastMathFlags::Reassoc;
          e = e.getFnArg(0);
          continue;
        }
        is_leaf = true;
      } else {
        is_leaf = true;
      }

      if (is_leaf) {
        n.operation = Node::Leaf;
        n.leaf      = std::move(e);
      } else {
        n.op1      = get(a);
        n.rounding = std::move(rounding);
      }

      return get(std::move(n));
    }
  }

  void saturate() {
    while (true) {
      auto nodes_copy = nodes;
      bool changed = false;

      for (auto &[node, n] : nodes_copy) {
        // canonicalize operand ids
        if ((node.op1 != -1u && node.op1 != root(node.op1)) ||
            (node.op2 != -1u && node.op2 != root(node.op2))) {
          Node nn = node;
          nn.op1 = root(node.op1);
          if (node.op2 != -1u)
            nn.op2 = root(node.op2);
          assert(is_neq(node <=> nn));

          decl_equivalent(std::move(nn), n);
          remove(node);
          changed = true;
          continue;
        }

        // commutativity: x . y == y . x
        // Always correct for add & mul regardless of fast-math
        if (node.operation == Node::Add || node.operation == Node::Mul) {
          Node nn = node;
          swap(nn.op1, nn.op2);
          decl_equivalent(std::move(nn), n);
        }

        if (node.flags & FastMathFlags::Reassoc) {
          // distributivity: x . (y + z) == x . y + x . z
          // TODO

          // associativity (x . y) . z == x . (y . z)
          // TODO

        }

        if (node.flags & FastMathFlags::Contract) {
          // x / y == x * (1 / y)
          // TODO
        }
      }

      if (!changed && nodes.size() == nodes_copy.size())
        return;
    }
  }

  expr smtOf(unsigned n) {
    vector<optional<expr>> exprs;  // map node id -> smt expr
    exprs.resize(nodes_eqs.size());
    vector<unsigned> todo = { n };

    do {
      unsigned node_id = todo.back();
      if (exprs[node_id]) {
        todo.pop_back();
        continue;
      }

      ChoiceExpr<expr> vals;
      bool has_all = true;

      for (auto *node : nodes_eqs[node_id]) {
        switch (node->operation) {
        case Node::Add:
        case Node::Sub:
        case Node::Mul:
        case Node::Div:
          if (!exprs[node->op2]) {
            todo.emplace_back(node->op2);
            has_all = false;
          }
          [[fallthrough]];

        case Node::Neg:
          if (!exprs[node->op1]) {
            todo.emplace_back(node->op1);
            has_all = false;
          }
          break;

        case Node::Leaf:
          break;
        }

        if (!has_all)
          continue;

        // TODO: handle other fastmath flags like nsz
        expr val;
        switch (node->operation) {
        case Node::Add:
          val = exprs[node->op1]->fadd(*exprs[node->op2], node->rounding);
          break;
        case Node::Sub:
          val = exprs[node->op1]->fsub(*exprs[node->op2], node->rounding);
          break;
        case Node::Mul:
          val = exprs[node->op1]->fmul(*exprs[node->op2], node->rounding);
          break;
        case Node::Div:
          val = exprs[node->op1]->fdiv(*exprs[node->op2], node->rounding);
          break;
        case Node::Neg:
          val = exprs[node->op1]->fneg();
          break;
        case Node::Leaf:
          val = node->leaf;
        }
        vals.add(std::move(val), expr(true));
      }

      if (!has_all)
        continue;

      auto [val, domain, qvar, pre] = std::move(vals)();
      assert(domain.isTrue());
      exprs[node_id] = std::move(val);
      // TODO: handle qvar, pre
      todo.pop_back();

    } while (!todo.empty());

    assert(exprs.size() >= n && exprs[n]);
    return std::move(*exprs[n]);
  }

#ifdef DEBUG_FMF
  friend ostream& operator<<(ostream &os, const EGraph &g) {
    for (unsigned i = 0, e = g.nodes_eqs.size(); i != e; ++i) {
      auto &nodes = g.nodes_eqs[i];
      if (!nodes.empty()) {
        os << "Root " << i << '\n';
        for (auto *n : nodes) {
          os << "  " << *n << '\n';
        }
      }
    }
    return os;
  }
#endif
};

}

namespace IR {

expr float_refined(const expr &a, const expr &b) {
  EGraph g;
  unsigned na = g.get(a);
  unsigned nb = g.get(b);
#ifdef DEBUG_FMF
  cout << "Before saturate:\n" << g << "Roots: " << na << " / " << nb << "\n\n";
#endif

  if (na == nb)
    return true;

  g.saturate();

  na = g.root(na);
  nb = g.root(nb);
#ifdef DEBUG_FMF
  cout << "After saturate:\n" << g << "Roots: " << na << " / " << nb << "\n\n";
#endif
  if (na == nb)
    return true;

#ifdef DEBUG_FMF
  cout << "Expr A: " << g.smtOf(na) << "\n\nExpr B: " << g.smtOf(nb) << "\n\n";
#endif

  return g.smtOf(na) == g.smtOf(nb);
}

}
