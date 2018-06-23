// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/function.h"
#include "ir/instr.h"

using namespace smt;
using namespace std;

namespace IR {

expr BasicBlock::getTypeConstraints() const {
  expr t(true);
  for (auto &i : instrs()) {
    t &= i.getTypeConstraints();
  }
  return t;
}

void BasicBlock::fixupTypes(const Model &m) {
  for (auto &i : m_instrs) {
    i->fixupTypes(m);
  }
}

void BasicBlock::addIntr(unique_ptr<Instr> &&i) {
  m_instrs.push_back(move(i));
}

ostream& operator<<(ostream &os, const BasicBlock &bb) {
  if (!bb.name.empty())
    os << bb.name << ":\n";
  for (auto &i : bb.instrs()) {
    os << "  ";
    i.print(os);
    os << '\n';
  }
  return os;
}


expr Function::getTypeConstraints() const {
  expr t(true);
  for (auto bb : getBBs()) {
    t &= bb->getTypeConstraints();
  }
  for (auto &l : { getConstants(), getInputs(), getUndefs() }) {
    for (auto &v : l) {
      t &= v.getTypeConstraints();
    }
  }
  return t;
}

void Function::fixupTypes(const Model &m) {
  for (auto bb : getBBs()) {
    bb->fixupTypes(m);
  }
  for (auto &l : { getConstants(), getInputs(), getUndefs() }) {
    for (auto &v : l) {
      const_cast<Value&>(v).fixupTypes(m);
    }
  }
}

BasicBlock& Function::getBB(string_view name) {
  auto p = BBs.try_emplace(string(name), name);
  if (p.second)
    BB_order.push_back(&p.first->second);
  return p.first->second;
}

const BasicBlock& Function::getBB(string_view name) const {
  return BBs.at(string(name));
}

void Function::addConstant(unique_ptr<Value> &&c) {
  constants.emplace_back(move(c));
}

void Function::addUndef(unique_ptr<UndefValue> &&u) {
  undefs.emplace_back(move(u));
}

void Function::addInput(unique_ptr<Input> &&i) {
  inputs.emplace_back(move(i));
}

Function::instr_iterator::
instr_iterator(vector<BasicBlock*>::const_iterator BBI,
               vector<BasicBlock*>::const_iterator BBE)
  : BBI(BBI), BBE(BBE) {
  next_bb();
}

void Function::instr_iterator::next_bb() {
  if (BBI != BBE) {
    auto BB_instrs = (*BBI)->instrs();
    II = BB_instrs.begin();
    IE = BB_instrs.end();
  }
}

void Function::instr_iterator::operator++(void) {
  if (++II != IE)
    return;
  ++BBI;
  next_bb();
}

void Function::print(ostream &os, bool print_header) const {
  if (print_header) {
    os << "declare @" << name << '(';
    bool first = true;
    for (auto &input : getInputs()) {
      if (!first)
        os << ", ";
      os << input;
      first = false;
    }
    os << ") {\n";
  }

  bool first = true;
  for (auto bb : BB_order) {
    if (!first)
      os << '\n';
    os << *bb;
    first = false;
  }

  if (print_header)
    os << "}\n";
}

ostream& operator<<(ostream &os, const Function &f) {
  f.print(os);
  return os;
}

}
