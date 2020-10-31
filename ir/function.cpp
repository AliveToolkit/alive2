// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/function.h"
#include "ir/instr.h"
#include "util/errors.h"
#include "util/sort.h"
#include "util/unionfind.h"
#include <fstream>
#include <set>
#include <unordered_set>

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

void BasicBlock::delInstr(Instr *i) {
  for (auto I = m_instrs.begin(), E = m_instrs.end(); I != E; ++I) {
    if (I->get() == i) {
      m_instrs.erase(I);
      return;
    }
  }
}

JumpInstr::it_helper BasicBlock::targets() const {
  if (empty())
    return {};
  if (auto jump = dynamic_cast<JumpInstr*>(m_instrs.back().get()))
    return jump->targets();
  return {};
}

void BasicBlock::replaceTargetWith(const BasicBlock *from,
                                   const BasicBlock *to) {
  if (auto jump = dynamic_cast<JumpInstr*>(&this->back())) {
    jump->replaceTargetWith(from, to);
  }
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

const BasicBlock* Function::getBBIfExists(std::string_view name) const {
  auto I = BBs.find(string(name));
  return I != BBs.end() ? &I->second : nullptr;
}

BasicBlock& Function::cloneBB(const BasicBlock &BB, const string &suffix,
                              unordered_map<const Value*, Value*> &vmap) {
  string bb_name = BB.getName() + suffix;
  auto &newbb = getBB(bb_name);
  for (auto &i : BB.instrs()) {
    auto d = i.dup(suffix);
    for (auto &op : d->operands()) {
      auto it = vmap.find(op);
      if (it != vmap.end()) {
        d->rauw(*op, *it->second);
      }
    }
    vmap[&i] = d.get();
    newbb.addInstr(move(d));
  }
  return newbb;
}

void Function::removeBB(BasicBlock &BB) {
  BBs.erase(BB.getName());

  for (auto I = BB_order.begin(), E = BB_order.end(); I != E; ++I) {
    if (*I == &BB) {
      BB_order.erase(I);
      break;
    }
  }
}

void Function::addConstant(unique_ptr<Value> &&c) {
  constants.emplace_back(move(c));
}

vector<GlobalVariable *> Function::getGlobalVars() const {
  vector<GlobalVariable *> gvs;
  for (auto I = constants.begin(), E = constants.end(); I != E; ++I) {
    if (auto *gv = dynamic_cast<GlobalVariable*>(I->get()))
      gvs.push_back(gv);
  }
  return gvs;
}

vector<string_view> Function::getGlobalVarNames() const {
  vector<string_view> gvnames;
  auto gvs = getGlobalVars();
  transform(gvs.begin(), gvs.end(), back_inserter(gvnames),
            [](auto &itm) { return string_view(itm->getName()).substr(1); });
  return gvnames;
}

void Function::addPredicate(unique_ptr<Predicate> &&p) {
  predicates.emplace_back(move(p));
}

void Function::addUndef(unique_ptr<UndefValue> &&u) {
  undefs.emplace_back(move(u));
}

void Function::addAggregate(unique_ptr<AggregateValue> &&a) {
  aggregates.emplace_back(move(a));
}

void Function::addInput(unique_ptr<Value> &&i) {
  assert(dynamic_cast<Input *>(i.get()) ||
         dynamic_cast<ConstantInput*>(i.get()));
  inputs.emplace_back(move(i));
}

bool Function::hasSameInputs(const Function &rhs) const {
  auto litr = inputs.begin(), lend = inputs.end();
  auto ritr = rhs.inputs.begin(), rend = rhs.inputs.end();

  auto skip_constinputs = [&]() {
    while (litr != lend && dynamic_cast<ConstantInput *>((*litr).get()))
      litr++;
    while (ritr != rend && dynamic_cast<ConstantInput *>((*ritr).get()))
      ritr++;
  };

  skip_constinputs();

  while (litr != lend && ritr != rend) {
    auto *lv = dynamic_cast<Input *>((*litr).get());
    auto *rv = dynamic_cast<Input *>((*ritr).get());
    // TODO: &lv->getType() != &rv->getType() doesn't work because
    // two struct types that are structurally equivalent don't compare equal
    if (lv->getAttributes() != rv->getAttributes()) {
      return false;
    }

    litr++;
    ritr++;
    skip_constinputs();
  }

  return litr == lend && ritr == rend;
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
  while (++BBI != BBE && (*BBI)->empty())
    ;
  next_bb();
}

multimap<Value*, Value*> Function::getUsers() const {
  multimap<Value*, Value*> users;
  for (auto &i : instrs()) {
    for (auto op : i.operands()) {
      users.emplace(op, const_cast<Instr*>(&i));
    }
  }
  for (auto &agg : aggregates) {
    for (auto val : agg->getVals()) {
      users.emplace(val, agg.get());
    }
  }
  for (auto &c : constants) {
    if (auto agg = dynamic_cast<AggregateValue*>(c.get())) {
      for (auto val : agg->getVals()) {
        users.emplace(val, agg);
      }
    }
  }
  return users;
}

template <typename T>
static bool removeUnused(T &data, const multimap<Value*, Value*> &users,
                         const vector<string_view> &src_glbs) {
  bool changed = false;
  for (auto I = data.begin(); I != data.end(); ) {
    if (users.count(I->get())) {
      ++I;
      continue;
    }

    // don't delete glbs in target that are used in src
    if (auto gv = dynamic_cast<GlobalVariable*>(I->get())) {
      auto name = string_view(gv->getName()).substr(1);
      if (find(src_glbs.begin(), src_glbs.end(), name) != src_glbs.end()) {
        ++I;
        continue;
      }
    }

    I = data.erase(I);
    changed = true;
  }
  return changed;
}

bool Function::removeUnusedStuff(const multimap<Value*, Value*> &users,
                                 const vector<string_view> &src_glbs) {
  bool changed = removeUnused(aggregates, users, src_glbs);
  changed |= removeUnused(constants, users, src_glbs);
  return changed;
}

void Function::topSort() {
  auto &bbs = getBBs();
  edgesTy edges(bbs.size());
  unordered_map<const BasicBlock*, unsigned> bb_map;

  unsigned i = 0;
  for (auto bb : bbs) {
    bb_map.emplace(bb, i++);
  }

  i = 0;
  for (auto bb : bbs) {
    for (auto &dst : bb->targets()) {
      edges[i].emplace(bb_map.at(&dst));
    }
    ++i;
  }

  vector<BasicBlock*> sorted_bbs;
  sorted_bbs.reserve(bbs.size());
  for (auto v : util::top_sort(edges)) {
    sorted_bbs.emplace_back(bbs[v]);
  }

  assert(sorted_bbs.size() == BB_order.size());
  BB_order = move(sorted_bbs);
}

void Function::unroll(unsigned k) {
  if (k == 0)
    return;

  LoopAnalysis la(*this);
  auto &roots = la.getRoots();
  if (roots.empty())
    return;

  auto &forest = la.getLoopForest();
  BasicBlock &sink = getBB("#sink");

  vector<tuple<BasicBlock*, unsigned, bool>> worklist;
  for (auto &root : roots) {
    worklist.emplace_back(root, 0, false);
  }

  // computed bottom-up during the post-order traversal below
  unordered_map<BasicBlock*, vector<BasicBlock*>> loop_nodes;

  // traverse each loop tree in post-order
  while (!worklist.empty()) {
    auto &[header, height, flag] = worklist.back();
    if (!flag) {
      flag = true;
      auto I = forest.find(header);
      if (I != forest.end()) {
        // process all non-leaf children first
        for (auto *child : I->second) {
          if (forest.count(child))
            worklist.emplace_back(child, height+1, false);
        }
        continue;
      }
    }
    worklist.pop_back();

    auto &loop_bbs = loop_nodes[header];
    loop_bbs.emplace_back(header);
    auto I = forest.find(header);
    if (I != forest.end()) {
      loop_bbs.insert(loop_bbs.end(), I->second.begin(), I->second.end());
    }

    auto *parent = la.getParent(header);
    auto *vparent = parent ? &loop_nodes[parent] : nullptr;
    if (vparent) {
      vparent->insert(vparent->end(), loop_bbs.begin(), loop_bbs.end());
    }

    // map: original BB -> {BB} U copies-of-BB
    unordered_map<const BasicBlock*, vector<BasicBlock*>> bbmap;
    for (auto *bb : loop_bbs) {
      bbmap[bb].emplace_back(bb);
    }

    // Clone BBs
    // Note that the BBs list must be iterated in top-sort order so that
    // values from previous BBs are available in vmap
    unordered_map<const Value*, Value*> vmap;
    string name_prefix;
    for (unsigned i = 0; i < height; ++i) {
      name_prefix += "#1";
    }
    for (unsigned unroll = 2; unroll <= k; ++unroll) {
      string suffix = name_prefix + '#' + to_string(unroll);
      for (auto *bb : loop_bbs) {
        auto &copies = bbmap.at(bb);
        copies.emplace_back(&cloneBB(*bb, suffix, vmap));
        if (vparent)
          vparent->emplace_back(copies.back());
      }
    }

    // Patch jump targets
    for (auto &[bb, copies] : bbmap) {
      for (unsigned unroll = 0; unroll < k; ++unroll) {
        auto *cloned = copies[unroll];
        for (auto &tgt : cloned->targets()) {
          // Loop exit; no patching needed
          if (!bbmap.count(&tgt))
            continue;

          const BasicBlock *to = nullptr;
          // handle backedge
          if (&tgt == header) {
            if (unroll == k-1)
              to = &sink;
            else
              to = bbmap[&tgt][unroll + 1];
          }
          // handle targets inside loop
          else {
            to = bbmap[&tgt][unroll];
          }
          cloned->replaceTargetWith(&tgt, to);
        }
      }
    }

    // TODO: handle phis
  }

  topSort();
}

void Function::print(ostream &os, bool print_header) const {
  {
    const auto &gvars = getGlobalVars();
    if (!gvars.empty()) {
      for (auto &v : gvars) {
        v->print(os);
        os << '\n';
      }
      os << '\n';
    }
  }

  if (print_header) {
    os << "define" << attrs << ' ' << getType() << " @" << name << '(';
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

void Function::writeDot(const char *filename_prefix) const {
  string fname = getName();
  if (filename_prefix)
    fname += string(".") + filename_prefix;

  CFG cfg(*const_cast<Function*>(this));
  {
    ofstream file(fname + ".cfg.dot");
    cfg.printDot(file);
  }
  {
    ofstream file(fname + ".dom.dot");
    DomTree(*const_cast<Function*>(this), cfg).printDot(file);
  }
  {
    ofstream file(fname + ".looptree.dot");
    LoopAnalysis(*const_cast<Function*>(this)).printDot(file);
  }
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

  for (auto [src, dst, instr] : *this) {
    (void)instr;
    os << '"' << bb_dot_name(src.getName()) << "\" -> \""
       << bb_dot_name(dst.getName()) << "\";\n";
  }
  os << "}\n";
}


// Relies on Alive's top_sort run during llvm2alive conversion in order to
// traverse the cfg in reverse postorder to build dominators.
void DomTree::buildDominators() {
  // initialization
  unsigned i = f.getBBs().size();
  for (auto &b : f.getBBs()) {
    doms.emplace(b, *b).first->second.order = --i;
  }

  // build predecessors relationship
  for (auto [src, tgt, instr] : cfg) {
    (void)instr;
    doms.at(&tgt).preds.push_back(&doms.at(&src));
  }

  auto &entry = doms.at(&f.getFirstBB());
  entry.dominator = &entry;

  // Cooper, Keith D.; Harvey, Timothy J.; and Kennedy, Ken (2001). 
  // A Simple, Fast Dominance Algorithm
  // http://www.cs.rice.edu/~keith/EMBED/dom.pdf
  // Makes multiple passes when CFG is cyclic to update incorrect initial
  // dominator guesses.
  bool changed;
  do {
    changed = false;
    for (auto &b : f.getBBs()) {
      auto &b_node = doms.at(b);
      if (b_node.preds.empty())
        continue;
      
      auto new_idom = b_node.preds.front();
      for (auto p : b_node.preds) {
        if (p->dominator != nullptr) {
          new_idom = intersect(p, new_idom);
        }
      }

      if (b_node.dominator != new_idom) {
        b_node.dominator = new_idom;
        changed = true;
      }
    }
  } while (changed);
}

DomTree::DomTreeNode* DomTree::intersect(DomTreeNode *f1, DomTreeNode *f2) {
  while (f1->order != f2->order) {
    while (f1->order < f2->order)
      f1 = f1->dominator;
    while (f2->order < f1->order)
      f2 = f2->dominator;
  }
  return f1;
}

// get immediate dominator BasicBlock
const BasicBlock* DomTree::getIDominator(const BasicBlock &bb) const {
  auto dom = doms.at(&bb).dominator;
  return dom ? &dom->bb : nullptr;
}

void DomTree::printDot(std::ostream &os) const {
  os << "digraph {\n"
        "\"" << bb_dot_name(f.getBBs()[0]->getName()) << "\" [shape=box];\n";

  for (auto I = f.getBBs().begin()+1, E = f.getBBs().end(); I != E; ++I) {
    if (auto dom = getIDominator(**I)) {
      os << '"' << bb_dot_name(dom->getName()) << "\" -> \""
         << bb_dot_name((*I)->getName()) << "\";\n";
    }
  }

  os << "}\n";
}


void LoopAnalysis::getDepthFirstSpanningTree() {
  unsigned bb_count = f.getBBs().size();
  node.resize(bb_count, nullptr);
  last.resize(bb_count, -1u);

  unsigned current = 0;
  vector<pair<BasicBlock*, bool>> worklist = { {&f.getFirstBB(), false} };
  unordered_set<const BasicBlock *> visited;
  while(!worklist.empty()) {
    auto &[bb, flag] = worklist.back();
    if (flag) {
      worklist.pop_back();
      last[number[bb]] = current - 1;
    } else {
      node[current] = bb;
      number[bb] = current++;
      flag = true;

      for (auto &tgt : bb->targets())
        if (visited.insert(&tgt).second)
          worklist.emplace_back(const_cast<BasicBlock*>(&tgt), false);
    }
  }
}

// Implemention of Tarjan-Havlak algorithm.
//
// Irreducible loops are partially supported.
//
// Tarjan, R. (1974). Testing Flow Graph Reducibility.
// Havlak, P. (1997). Nesting of reducible and irreducible loops.
void LoopAnalysis::run() {
  getDepthFirstSpanningTree();
  unsigned bb_count = f.getBBs().size();

  auto isAncestor = [this](unsigned w, unsigned v) -> bool {
    return w <= v && v <= last[w];
  };

  vector<set<unsigned>> nonBackPreds(bb_count), backPreds(bb_count);
  header.resize(bb_count, 0);
  type.resize(bb_count, NodeType::nonheader);

  for (auto [src, dst, instr] : cfg) {
    unsigned v = number.at(&src), w = number.at(&dst);
    if (isAncestor(w, v))
      backPreds[w].insert(v);
    else
      nonBackPreds[w].insert(v);
  }

  UnionFind uf(bb_count);

  for (unsigned w = bb_count - 1; w != -1u; --w) {
    set<unsigned> P;
    for (unsigned v : backPreds[w])
      if (v != w)
        P.insert(uf.find(v));
      else
        type[w] = NodeType::self;

    if (!P.empty())
      type[w] = NodeType::reducible;

    set<unsigned> workList(P);
    while (!workList.empty()) {
      auto I = workList.begin();
      unsigned x = *I;
      workList.erase(I);

      for (unsigned y : nonBackPreds[x]) {
        unsigned yy = uf.find(y);
        if (!isAncestor(w, yy)) {
          type[w] = NodeType::irreducible;
          nonBackPreds[w].insert(yy);
        } else if (yy != w && !P.count(yy)) {
          P.insert(yy);
          workList.insert(yy);
        }
      }
    }

    for (unsigned x : P) {
      header[x] = w;
      uf.merge(x, w);
    }
  }

  // Construct the loop forest (0 or more loop trees)
  for (unsigned i = 0; i < bb_count; ++i) {
    auto h = header[i];
    if (h == 0 && type[i] != nonheader) {
      roots.emplace_back(node[i]);
      (void)forest[node[i]];
    }
    else if (h != 0 || type[i] != nonheader) {
      parent.emplace(node[i], node[h]);
      forest[node[h]].emplace_back(node[i]);
    }
  }

  // sort nodes with DFST pre-order so that BB cloning won't break dominance
  for (auto &[_, nodes] : forest) {
    sort(nodes.begin(), nodes.end(),
      [&](const BasicBlock *a, const BasicBlock *b) -> bool {
        return number.at(a) < number.at(b);
      }
    );
  }
}

BasicBlock* LoopAnalysis::getParent(BasicBlock *bb) const {
  auto I = parent.find(bb);
  return I != parent.end() ? I->second : nullptr;
}

void LoopAnalysis::printDot(ostream &os) const {
  os << "digraph {\n";

  unordered_set<const BasicBlock*> seen_roots;
  auto decl_root = [&](const BasicBlock *root) {
    auto name = bb_dot_name(root->getName());
    if (seen_roots.insert(root).second)
      os << '"' << name << "\" [shape=square];\n";
    return name;
  };

  for (auto &[root, nodes] : forest) {
    auto root_name = decl_root(root);
    for (auto *node : nodes) {
      os << '"' << root_name << "\" -> \""
         << bb_dot_name(node->getName()) << "\";\n";
    }
  }
  os << "}\n";
}

} 

