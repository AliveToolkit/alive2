#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/attrs.h"
#include "ir/instr.h"
#include "ir/precondition.h"
#include "ir/value.h"
#include "smt/expr.h"
#include "util/compiler.h"
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace smt { class Model; }

namespace IR {

class Function;

class BasicBlock final {
  std::string name;
  std::vector<std::unique_ptr<Instr>> m_instrs;

  // If the basic block is a header, this holds all exit blocks of its loop
  // TODO: remove this..
  std::unordered_set<BasicBlock*> exit_blocks;

public:
  BasicBlock(std::string_view name) : name(name) {}

  const std::string& getName() const { return name; }

  smt::expr getTypeConstraints(const Function &f) const;
  void fixupTypes(const smt::Model &m);

  void addInstr(std::unique_ptr<Instr> &&i, bool push_front = false);
  void addInstrAt(std::unique_ptr<Instr> &&i, const Instr *other, bool before);
  void delInstr(const Instr *i);

  void addExitBlock(BasicBlock* bb);
  const std::unordered_set<BasicBlock*>& getExitBlocks() const {
    return exit_blocks;
  }

  util::const_strip_unique_ptr<decltype(m_instrs)> instrs() const {
    return m_instrs;
  }
  Instr& back() { return *m_instrs.back(); }
  std::vector<Phi*> phis() const;

  bool empty() const { return m_instrs.empty(); }
  JumpInstr::it_helper targets() const;
  void replaceTargetWith(const BasicBlock *from, const BasicBlock *to);

  std::unique_ptr<BasicBlock> dup(Function &f, const std::string &suffix) const;
  void rauw(const Value &what, Value &with);

  friend std::ostream& operator<<(std::ostream &os, const BasicBlock &bb);
};


class Function final {
  IR::Type *type = nullptr;
  std::string name;
  std::unordered_map<std::string, BasicBlock> BBs;
  std::vector<BasicBlock*> BB_order;

  static BasicBlock sink_bb;

  unsigned bits_pointers = 64;
  unsigned bits_ptr_offset = 64;
  bool little_endian = true;
  bool is_var_args = false;

  // constants used in this function
  std::vector<std::unique_ptr<Value>> constants;
  std::vector<std::unique_ptr<Predicate>> predicates;
  std::vector<std::unique_ptr<Value>> undefs;
  std::vector<std::unique_ptr<AggregateValue>> aggregates;
  std::vector<std::unique_ptr<Value>> inputs;

  // an input that has 'returned' attribute
  Value *returned_input = nullptr;

  FnAttrs attrs;

  // TODO: Move this to a 'program' class
public:
  struct FnDecl {
    std::string name;
    std::vector<std::pair<Type*, ParamAttrs>> inputs;
    Type *output;
    FnAttrs attrs;
  };
private:
  std::vector<FnDecl> fn_decls;

public:
  Function() = default;
  Function(Type &type, std::string &&name, unsigned bits_pointers = 64,
           unsigned bits_ptr_offset = 64, bool little_endian = true,
           bool is_var_args = false)
    : type(&type), name(std::move(name)), bits_pointers(bits_pointers),
      bits_ptr_offset(bits_ptr_offset), little_endian(little_endian),
      is_var_args(is_var_args) {}

  const IR::Type& getType() const { return type ? *type : Type::voidTy; }
  void setType(IR::Type &t) { type = &t; }

  const std::string& getName() const { return name; }

  auto& getFnAttrs() { return attrs; }
  auto& getFnAttrs() const { return attrs; }

  smt::expr getTypeConstraints() const;
  void fixupTypes(const smt::Model &m);
  void rauw(const Value &what, Value &with);

  const BasicBlock& getFirstBB() const { return *BB_order[0]; }
  BasicBlock& getFirstBB() { return *BB_order[0]; }
  const BasicBlock& getSinkBB() const { return sink_bb; }
  BasicBlock& getBB(std::string_view name, bool push_front = false);
  const BasicBlock& getBB(std::string_view name) const;
  const BasicBlock& bbOf(const Instr &i) const;

  BasicBlock& insertBBAfter(std::string_view name, const BasicBlock &bb);

  void removeBB(BasicBlock &BB);

  void addConstant(std::unique_ptr<Value> &&c);
  util::const_strip_unique_ptr<decltype(constants)> getConstants() const {
    return constants;
  }
  unsigned numConstants() const { return constants.size(); }
  Value &getConstant(int idx) const { return *constants[idx]; }
  Value* getConstant(std::string_view name) const;

  std::vector<GlobalVariable *> getGlobalVars() const;
  std::vector<std::string_view> getGlobalVarNames() const;

  void addPredicate(std::unique_ptr<Predicate> &&p);

  void addUndef(std::unique_ptr<UndefValue> &&c);
  util::const_strip_unique_ptr<decltype(undefs)> getUndefs() const {
    return undefs;
  }

  void addAggregate(std::unique_ptr<AggregateValue> &&a);

  void addInput(std::unique_ptr<Value> &&c);
  void replaceInput(std::unique_ptr<Value> &&c, unsigned idx);
  Value &getInput(int idx) { return *inputs[idx]; }
  util::const_strip_unique_ptr<decltype(inputs)> getInputs() const {
    return inputs;
  }
  Value *getReturnedInput() const { return returned_input; }
  void setReturnedInput(Value *v) { returned_input = v; }

  bool hasReturn() const;
  unsigned bitsPointers() const { return bits_pointers; }
  unsigned bitsPtrOffset() const { return bits_ptr_offset; }
  bool isLittleEndian() const { return little_endian; }
  bool isVarArgs() const { return is_var_args; }

  void syncDataWithSrc(Function &src);

  auto& getBBs() { return BB_order; }
  const auto& getBBs() const { return BB_order; }

  class instr_iterator {
    std::vector<BasicBlock*>::const_iterator BBI, BBE;
    util::const_strip_unique_ptr<std::vector<std::unique_ptr<Instr>>>::
      const_iterator II, IE;
    void next_bb();
  public:
    instr_iterator(std::vector<BasicBlock*>::const_iterator &&BBI,
                   std::vector<BasicBlock*>::const_iterator &&BBE);
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
  instr_helper instrs() { return *this; }
  instr_helper instrs() const { return *this; }

  void addFnDecl(FnDecl &&decl);
  auto& getFnDecls() const { return fn_decls; }

  using UsersTy = std::unordered_map<const Value*,
                                     std::set<std::pair<Value*, BasicBlock*>>>;
  UsersTy getUsers() const;
  bool removeUnusedStuff(const UsersTy &users,
                         const std::vector<std::string_view> &src_glbs);

  void topSort();
  void unroll(unsigned k);

  void print(std::ostream &os, bool print_header = true) const;
  friend std::ostream &operator<<(std::ostream &os, const Function &f);
  void writeDot(const char *filename_prefix) const;
};


class CFG final {
  Function &f;

public:
  CFG(Function &f) : f(f) {}

  class edge_iterator {
    std::vector<BasicBlock*>::iterator bbi, bbe;
    JumpInstr::target_iterator ti, te;
    void next();

  public:
    edge_iterator(std::vector<BasicBlock*>::iterator &&it,
                  std::vector<BasicBlock*>::iterator &&end);
    std::tuple<const BasicBlock&, const BasicBlock&, const Instr&>
      operator*() const;
    void operator++(void);
    bool operator!=(edge_iterator &rhs) const;
  };

  edge_iterator begin() const {
    return { f.getBBs().begin(), f.getBBs().end() };
  }
  edge_iterator end() const { return { f.getBBs().end(), f.getBBs().end() }; }

  void printDot(std::ostream &os) const;
};


class DomTree final {
  const Function &f;

  struct DomTreeNode {
    const BasicBlock &bb;
    std::vector<DomTreeNode*> preds;
    DomTreeNode *dominator = nullptr;
    unsigned order;

    DomTreeNode(const BasicBlock &bb) : bb(bb) {}
  };

  std::unordered_map<const BasicBlock*, DomTreeNode> doms;

  void buildDominators(const CFG &cfg);
  static DomTreeNode* intersect(DomTreeNode *b1, DomTreeNode *b2);

public:
  DomTree(const Function &f, const CFG &cfg) : f(f) { buildDominators(cfg); }
  const BasicBlock* getIDominator(const BasicBlock &bb) const;
  bool dominates(const BasicBlock *a, const BasicBlock *b) const;
  void printDot(std::ostream &os) const;
};


class LoopAnalysis final {
  Function &f;
  CFG cfg;

  std::unordered_map<const BasicBlock*, unsigned> number;
  std::vector<BasicBlock*> node;
  std::vector<unsigned> last;
  void getDepthFirstSpanningTree();

  std::vector<unsigned> header;
  enum NodeType { nonheader, self, reducible, irreducible };
  std::vector<NodeType> type;
  std::unordered_map<BasicBlock*, std::vector<BasicBlock*>> forest;
  std::unordered_map<BasicBlock*, BasicBlock*> parent;
  std::vector<BasicBlock*> roots;
  void run();

public:
  LoopAnalysis(Function &f) : f(f), cfg(f) { run(); }

  auto& getRoots() const { return roots; }
  auto& getLoopForest() const { return forest; }
  BasicBlock* getParent(BasicBlock *bb) const;

  void printDot(std::ostream &os) const;
};

}
