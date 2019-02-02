#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/constant.h"
#include "ir/instr.h"
#include "ir/value.h"
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

class Function;

class BasicBlock final {
  std::string name;
  std::vector<std::unique_ptr<Instr>> m_instrs;

public:
  BasicBlock(std::string &&name) : name(std::move(name)) {}
  BasicBlock(std::string_view name) : name(name) {}

  const std::string& getName() const { return name; }

  smt::expr getTypeConstraints(const Function &f) const;
  void fixupTypes(const smt::Model &m);

  void addInstr(std::unique_ptr<Instr> &&i);

  util::const_strip_unique_ptr<decltype(m_instrs)> instrs() const {
    return m_instrs;
  }
  Instr& back() { return *m_instrs.back(); }

  friend std::ostream& operator<<(std::ostream &os, const BasicBlock &bb);
};


class Function final {
  IR::Type *type = nullptr;
  std::string name;
  std::unordered_map<std::string, BasicBlock> BBs;
  std::vector<BasicBlock*> BB_order;
  Predicate *precondition = nullptr;

  // constants used in this function
  std::vector<std::unique_ptr<Value>> constants;
  std::vector<std::unique_ptr<Predicate>> predicates;
  std::vector<std::unique_ptr<Value>> undefs;
  std::vector<std::unique_ptr<Value>> inputs;

public:
  Function() {}
  Function(Type &type, std::string &&name)
    : type(&type), name(std::move(name)) {}

  const IR::Type& getType() const { return type ? *type : Type::voidTy; }
  void setType(IR::Type &t) { type = &t; }

  const std::string& getName() const { return name; }

  smt::expr getTypeConstraints() const;
  void fixupTypes(const smt::Model &m);

  const BasicBlock& getFirstBB() const { return *BB_order[0]; }
  BasicBlock& getBB(std::string_view name);
  const BasicBlock& getBB(std::string_view name) const;

  void addConstant(std::unique_ptr<Value> &&c);
  util::const_strip_unique_ptr<decltype(constants)> getConstants() const {
    return constants;
  }

  void addPredicate(std::unique_ptr<Predicate> &&p);

  void addUndef(std::unique_ptr<UndefValue> &&c);
  util::const_strip_unique_ptr<decltype(undefs)> getUndefs() const {
    return undefs;
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
