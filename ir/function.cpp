// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/function.h"
#include "ir/instr.h"
#include "util/errors.h"

using namespace smt;
using namespace util;
using namespace std;

namespace IR {

expr BasicBlock::getTypeConstraints(const Function &f) const {
  expr t(true);
  for (auto &i : instrs()) {
    t &= i.getTypeConstraints(f);
  }
  return t;
}

void BasicBlock::fixupTypes(const Model &m) {
  for (auto &i : m_instrs) {
    i->fixupTypes(m);
  }
}

void BasicBlock::addInstr(unique_ptr<Instr> &&i) {
  m_instrs.push_back(move(i));
}

unique_ptr<BasicBlock> BasicBlock::dup(const string &suffix) const {
  auto newbb = make_unique<BasicBlock>(name + suffix);
  for (auto &i : instrs()) {
    newbb->addInstr(i.dup(suffix));
  }
  return newbb;
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
    t &= bb->getTypeConstraints(*this);
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

BasicBlock& Function::getBB(string_view name, bool push_front) {
  auto p = BBs.try_emplace(string(name), name);
  if (p.second) {
    if (push_front)
      BB_order.insert(BB_order.begin(), &p.first->second);
    else
      BB_order.push_back(&p.first->second);
  }
  return p.first->second;
}

const BasicBlock& Function::getBB(string_view name) const {
  return BBs.at(string(name));
}

void Function::addConstant(unique_ptr<Value> &&c) {
  constants.emplace_back(move(c));
}

vector<GlobalVariable *> Function::getGlobalVars() const {
  vector<GlobalVariable *> gvs;
  for (auto I = constants.begin(), E = constants.end(); I != E; ++I) {
    const unique_ptr<Value> &c = *I;
    if (auto *gv = dynamic_cast<GlobalVariable *>(c.get()))
      gvs.push_back(gv);
  }
  return gvs;
}

vector<string_view> Function::getGlobalVarNames() const {
  vector<string_view> gvnames;
  auto gvs = getGlobalVars();
  transform(gvs.begin(), gvs.end(), back_inserter(gvnames),
            [](auto itm) { return string_view(itm->getName()).substr(1); });
  return gvnames;
}

void Function::addPredicate(unique_ptr<Predicate> &&p) {
  predicates.emplace_back(move(p));
}

void Function::addUndef(unique_ptr<UndefValue> &&u) {
  undefs.emplace_back(move(u));
}

void Function::addInput(unique_ptr<Value> &&i) {
  assert(dynamic_cast<Input *>(i.get()) ||
         dynamic_cast<ConstantInput*>(i.get()));
  inputs.emplace_back(move(i));
}

bool Function::hasReturn() const {
  for (auto &i : instrs()) {
    if (dynamic_cast<const Return *>(&i))
      return true;
  }
  return false;
}

void Function::syncDataWithSrc(const Function &src) {
  auto IS = src.inputs.begin(), ES = src.inputs.end();
  auto IT = inputs.begin(), ET = inputs.end();

  for (; IS != ES && IT != ET; ++IS, ++IT) {
    if (auto in_tgt = dynamic_cast<Input*>(IT->get()))
      in_tgt->copySMTName(*dynamic_cast<Input*>(IS->get()));

    if (!(IS->get()->getType() == IT->get()->getType()).isTrue())
      throw AliveException("Source and target args have different type", false);
  }

  if (IS != ES || IT != ET)
    throw AliveException("Source and target have different number of args",
                         false);
}

Function::instr_iterator::
instr_iterator(vector<BasicBlock*>::const_iterator &&BBI,
               vector<BasicBlock*>::const_iterator &&BBE)
  : BBI(move(BBI)), BBE(move(BBE)) {
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
    os << "define " << getType() << " @" << name << '(';
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


void CFG::edge_iterator::next() {
  // jump to next BB with a terminator that is a jump
  while (true) {
    if (bbi == bbe)
      return;

    if (auto instr = dynamic_cast<JumpInstr*>(&(*bbi)->back())) {
      ti = instr->targets().begin();
      te = instr->targets().end();
      return;
    }

    ++bbi;
  }
}

CFG::edge_iterator::edge_iterator(vector<BasicBlock*>::iterator &&it,
                                  vector<BasicBlock*>::iterator &&end)
  : bbi(move(it)), bbe(move(end)) {
  next();
}

tuple<const BasicBlock&, const BasicBlock&, const Instr&>
  CFG::edge_iterator::operator*() const {
  return { **bbi, *ti, (*bbi)->back() };
}

void CFG::edge_iterator::operator++(void) {
  if (++ti == te) {
    ++bbi;
    next();
  }
}

bool CFG::edge_iterator::operator!=(edge_iterator &rhs) const {
  return bbi != rhs.bbi && (bbi == bbe || rhs.bbi == rhs.bbe || ti != rhs.ti);
}

static string_view bb_dot_name(const string &name) {
  if (name[0] == '%')
    return string_view(name).substr(1);
  return name;
}

void CFG::printDot(ostream &os) const {
  os << "digraph {\n"
        "\"" << bb_dot_name(f.getBBs()[0]->getName()) << "\" [shape=box];\n";

  for (const auto &[src, dst, instr] : *this) {
    (void)instr;
    os << '"' << bb_dot_name(src.getName()) << "\" -> \""
       << bb_dot_name(dst.getName()) << "\";\n";
  }
  os << "}\n";
}

}
