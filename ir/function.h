#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/instr.h"
#include "smt/expr.h"
#include "util/compiler.h"
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace smt { class Model; }

namespace IR {

class BasicBlock {
  std::string name;
  std::vector<std::unique_ptr<Instr>> m_instrs;

public:
  BasicBlock(std::string &&name) : name(std::move(name)) {}
  BasicBlock(std::string_view name) : name(name) {}

  smt::expr getTypeConstraints() const;
  void fixupTypes(const smt::Model &m);

  void addIntr(std::unique_ptr<Instr> &&i);

  util::const_strip_unique_ptr<decltype(m_instrs)> instrs() const {
    return m_instrs;
  }

  friend std::ostream& operator<<(std::ostream &os, const BasicBlock &bb);
};


class Function {
  std::string name;
  std::unordered_map<std::string, BasicBlock> BBs;
  std::vector<BasicBlock*> BB_order;

  // constants used in this function
  std::vector<std::unique_ptr<Value>> constants;
  std::vector<std::unique_ptr<Value>> inputs;

public:
  Function() {}
  Function(std::string &&name) : name(std::move(name)) {}

  smt::expr getTypeConstraints() const;
  void fixupTypes(const smt::Model &m);

  BasicBlock& getBB(std::string_view name);
  const BasicBlock& getBB(std::string_view name) const;

  void addConstant(std::unique_ptr<Value> &&c);
  util::const_strip_unique_ptr<decltype(constants)> getConstants() const {
    return constants;
  }

  void addInput(std::unique_ptr<Input> &&c);
  util::const_strip_unique_ptr<decltype(inputs)> getInputs() const {
    return inputs;
  }

  auto& getBBs() { return BB_order; }
  const auto& getBBs() const { return BB_order; }

  class instr_iterator {
    std::vector<BasicBlock*>::const_iterator BBI, BBE;
    util::const_strip_unique_ptr<std::vector<std::unique_ptr<Instr>>>::
      const_iterator II, IE;
    void next_bb();
  public:
    instr_iterator(std::vector<BasicBlock*>::const_iterator BBI,
                   std::vector<BasicBlock*>::const_iterator BBE);
    const IR::Instr& operator*() const { return *II; }
    void operator++(void);
    bool operator!=(instr_iterator &rhs) const { return BBI != rhs.BBI; }
  };

  class instr_helper {
    const Function &f;
  public:
    instr_helper(const Function &f) : f(f) {}
    instr_iterator begin() { return { f.BB_order.begin(), f.BB_order.end() }; }
    instr_iterator end()   { return { f.BB_order.end(), f.BB_order.end() }; }
  };
  instr_helper instrs() { return { *this }; }
  instr_helper instrs() const { return { *this }; }

  void print(std::ostream &os, bool print_header = true) const;
  friend std::ostream &operator<<(std::ostream &os, const Function &f);
};

}
