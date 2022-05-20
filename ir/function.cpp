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

void BasicBlock::addInstr(unique_ptr<Instr> &&i, bool push_front) {
  if (push_front)
    m_instrs.emplace(m_instrs.begin(), std::move(i));
  else
    m_instrs.emplace_back(std::move(i));
}

void BasicBlock::addInstrAt(unique_ptr<Instr> &&i, const Instr *other,
                            bool before) {
  for (auto I = m_instrs.begin(); true; ++I) {
    assert(I != m_instrs.end());
    if (I->get() == other) {
      if (!before)
        ++I;
      m_instrs.emplace(I, std::move(i));
      break;
    }
  }
}

void BasicBlock::delInstr(Instr *i) {
  for (auto I = m_instrs.begin(), E = m_instrs.end(); I != E; ++I) {
    if (I->get() == i) {
      m_instrs.erase(I);
      return;
    }
  }
}

vector<Phi*> BasicBlock::phis() const {
  vector<Phi*> phis;
  for (auto &i : m_instrs) {
    if (auto phi = dynamic_cast<Phi*>(i.get()))
      phis.emplace_back(phi);
  }
  return phis;
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

void BasicBlock::rauw(const Value &what, Value &with) {
  for (auto &i : m_instrs) {
    i->rauw(what, with);
  }
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


BasicBlock Function::sink_bb("#sink");

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
  assert(name != "#sink");
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
  assert(name != "#sink");
  return BBs.at(string(name));
}

const BasicBlock& Function::bbOf(const Instr &i) const {
  for (auto *bb : getBBs()) {
    for (auto &ii : bb->instrs())
      if (&ii == &i)
        return *bb;
  }
  UNREACHABLE();
}

BasicBlock& Function::insertBBAfter(string_view name, const BasicBlock &bb) {
  auto p = BBs.try_emplace(string(name), name);
  if (p.second) {
    auto I = find(BB_order.begin(), BB_order.end(), &bb);
    assert(I != BB_order.end());
    BB_order.insert(next(I), &p.first->second);
  }
  return p.first->second;
}

void Function::removeBB(BasicBlock &BB) {
  assert(BB.getName() != "#sink");
  BBs.erase(BB.getName());

  for (auto I = BB_order.begin(), E = BB_order.end(); I != E; ++I) {
    if (*I == &BB) {
      BB_order.erase(I);
      break;
    }
  }
}

void Function::addConstant(unique_ptr<Value> &&c) {
  constants.emplace_back(std::move(c));
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
  predicates.emplace_back(std::move(p));
}

void Function::addUndef(unique_ptr<UndefValue> &&u) {
  undefs.emplace_back(std::move(u));
}

void Function::addAggregate(unique_ptr<AggregateValue> &&a) {
  aggregates.emplace_back(std::move(a));
}

void Function::addInput(unique_ptr<Value> &&i) {
  assert(dynamic_cast<Input *>(i.get()) ||
         dynamic_cast<ConstantInput*>(i.get()));
  inputs.emplace_back(std::move(i));
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
  : BBI(std::move(BBI)), BBE(std::move(BBE)) {
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

Function::UsersTy Function::getUsers() const {
  UsersTy users;
  for (auto *bb : getBBs()) {
    for (auto &i : bb->instrs()) {
      for (auto op : i.operands()) {
        users[op].emplace(const_cast<Instr*>(&i), bb);
      }
    }
  }
  for (auto &agg : aggregates) {
    for (auto val : agg->getVals()) {
      users[val].emplace(agg.get(), nullptr);
    }
  }
  for (auto &c : constants) {
    if (auto agg = dynamic_cast<AggregateValue*>(c.get())) {
      for (auto val : agg->getVals()) {
        users[val].emplace(agg, nullptr);
      }
    }
  }
  return users;
}

template <typename T>
static bool removeUnused(T &data, const Function::UsersTy &users,
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

bool Function::removeUnusedStuff(const UsersTy &users,
                                 const vector<string_view> &src_glbs) {
  bool changed = removeUnused(aggregates, users, src_glbs);
  changed |= removeUnused(constants, users, src_glbs);
  return changed;
}

static vector<BasicBlock*> top_sort(const vector<BasicBlock*> &bbs) {
  edgesTy edges(bbs.size());
  unordered_map<const BasicBlock*, unsigned> bb_map;

  unsigned i = 0;
  for (auto bb : bbs) {
    bb_map.emplace(bb, i++);
  }

  i = 0;
  for (auto bb : bbs) {
    for (auto &dst : bb->targets()) {
      auto dst_I = bb_map.find(&dst);
      if (dst_I != bb_map.end())
        edges[i].emplace(dst_I->second);
    }
    ++i;
  }

  vector<BasicBlock*> sorted_bbs;
  sorted_bbs.reserve(bbs.size());
  for (auto v : util::top_sort(edges)) {
    sorted_bbs.emplace_back(bbs[v]);
  }

  assert(sorted_bbs.size() == bbs.size());
  return sorted_bbs;
}

void Function::topSort() {
  BB_order = top_sort(BB_order);
}

static BasicBlock&
cloneBB(Function &F, const BasicBlock &BB, const char *suffix,
        const unordered_map<const BasicBlock*, vector<BasicBlock*>> &bbmap,
        unordered_map<const Value*, vector<pair<BasicBlock*, Value*>>> &vmap) {
  string bb_name = BB.getName() + suffix;
  auto &newbb = F.getBB(bb_name);
  unordered_set<const Value *> phis_from_orig_bb;

  for (auto &i : BB.instrs()) {
    if (dynamic_cast<const Phi *>(&i))
      phis_from_orig_bb.insert(&i);

    auto d = i.dup(suffix);
    for (auto &op : d->operands()) {
      auto it = vmap.find(op);
      if (it != vmap.end()) {
        // consider this case:
        //   loop:
        //     %op = phi ...
        //     %k  = phi [%op, %loop], ...
        //
        // In iteration i, %k should point to iteration (i-1)'s %k.
        // If is_phi_to_phi is true, %op is the phi in this block.
        bool is_phi_to_phi = dynamic_cast<const Phi *>(&i) &&
                             phis_from_orig_bb.count(op);
        if (is_phi_to_phi) {
          if (it->second.size() >= 2) {
            d->rauw(*op, *it->second[it->second.size()-2].second);
          }
        } else {
          d->rauw(*op, *it->second.back().second);
        }
      }
    }
    if (!i.isVoid())
      vmap[&i].emplace_back(&newbb, d.get());
    newbb.addInstr(std::move(d));
  }

  for (auto *phi : newbb.phis()) {
    for (auto &src : phi->sources()) {
      bool replaced = false;
      for (auto &[bb, copies] : bbmap) {
        if (src == bb->getName()) {
          phi->replaceSourceWith(src, copies.back()->getName());
          replaced = true;
          break;
        }
      }
      if (!replaced) {
        phi->removeValue(src);
      }
    }

    // If a phi becomes empty this can be a multi-entry loop without
    // loop-carried dependencies like:
    // loop:
    //   phi [x, entry1], [y, entry2]
    //   ...
    //   br loop
    if (phi->getValues().empty()) {
      for (auto &[val, copies] : vmap) {
        if (copies.back().second == phi) {
          newbb.rauw(*phi, *const_cast<Value*>(val));
          copies.pop_back();
          if (copies.empty())
            vmap.erase(val);
          break;
        }
      }
      newbb.delInstr(phi);
    }
  }

  return newbb;
}

static auto getPhiPredecessors(const Function &F) {
  unordered_map<string, vector<pair<Phi*, Value*>>> map;
  for (auto &i : F.instrs()) {
    if (auto phi = dynamic_cast<const Phi*>(&i)) {
      for (auto &[val, pred] : phi->getValues()) {
        map[pred].emplace_back(const_cast<Phi*>(phi), const_cast<Value*>(val));
      }
    }
  }
  return map;
}

void Function::unroll(unsigned k) {
  if (k == 0)
    return;

  LoopAnalysis la(*this);
  auto &roots = la.getRoots();
  if (roots.empty())
    return;

  auto &forest = la.getLoopForest();
  auto &sink = getSinkBB();

  vector<tuple<BasicBlock*, unsigned, bool>> worklist;
  // insert in reverse order because the worklist is iterated in LIFO
  for (auto I = roots.rbegin(), E = roots.rend(); I != E; ++I) {
    worklist.emplace_back(*I, 0, false);
  }

  // computed bottom-up during the post-order traversal below
  unordered_map<BasicBlock*, vector<BasicBlock*>> loop_nodes;

  // grab all value users before duplication so the list is shorter
  auto users = getUsers();
  auto phi_preds = getPhiPredecessors(*this);

  // traverse each loop tree in post-order
  while (!worklist.empty()) {
    auto [header, height, flag] = worklist.back();
    if (!flag) {
      get<2>(worklist.back()) = flag = true;
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

    vector<BasicBlock*> loop_bbs = { header };
    vector<BasicBlock*> own_loop_bbs = loop_bbs;
    auto I = forest.find(header);
    if (I != forest.end()) {
      for (auto *bb : top_sort(I->second)) {
        auto II = loop_nodes.find(bb);
        if (II != loop_nodes.end()) {
          loop_bbs.insert(loop_bbs.end(), II->second.begin(), II->second.end());
        } else {
          loop_bbs.emplace_back(bb);
        }
        own_loop_bbs.emplace_back(bb);
      }
    }

    // map: original BB -> {BB} U copies-of-BB
    unordered_map<const BasicBlock*, vector<BasicBlock*>> bbmap;
    for (auto *bb : loop_bbs) {
      bbmap[bb].emplace_back(bb);
    }

    // loop exit BB -> landing outside-loop BB
    set<pair<BasicBlock*, BasicBlock*>> exit_edges;
    for (auto *bb : loop_bbs) {
      for (auto &dst : bb->targets()) {
        if (!bbmap.count(&dst)) {
          exit_edges.emplace(bb, const_cast<BasicBlock*>(&dst));
        }
      }
    }

    // Clone BBs
    // Note that the BBs list must be iterated in top-sort order so that
    // values from previous BBs are available in vmap
    auto &unrolled_bbs = loop_nodes.emplace(header, loop_bbs).first->second;
    unordered_map<const Value*, vector<pair<BasicBlock*, Value*>>> vmap;
    string name_prefix;
    for (unsigned i = 0; i < height; ++i) {
      name_prefix += "#1";
    }
    for (unsigned unroll = 2; unroll <= k; ++unroll) {
      string suffix = name_prefix + '#' + to_string(unroll);
      for (auto *bb : loop_bbs) {
        auto &copies = bbmap.at(bb);
        copies.emplace_back(&cloneBB(*this, *bb, suffix.c_str(), bbmap, vmap));
        unrolled_bbs.emplace_back(copies.back());
      }
    }

    // Clone the header once more so that the last iteration of the loop can
    // exit. Otherwise the last iteration would be wasted.
    // Here we assume the header is an exit, as that's the common case.
    // If not, this extra duplication is wasteful.
    if (bbmap.size() > 1) {
      auto &copies = bbmap.at(header);
      copies.emplace_back(&cloneBB(*this, *header, "#exit", bbmap, vmap));
      unrolled_bbs.emplace_back(copies.back());
    }

    // Patch jump targets
    for (auto &[bb, copies] : bbmap) {
      for (unsigned unroll = 0, e = copies.size(); unroll < e; ++unroll) {
        auto *cloned = copies[unroll];
        for (auto &tgt : cloned->targets()) {
          // Loop exit; no patching needed
          if (!bbmap.count(&tgt))
            continue;

          const BasicBlock *to = nullptr;
          auto &dst_vect = bbmap[&tgt];
          auto dst_unroll = dst_vect.size();

          // handle backedge
          if (&tgt == header) {
            to = unroll+1 < dst_unroll ? dst_vect[unroll + 1] : &sink;
          }
          // handle targets inside loop
          else {
            to = unroll < dst_unroll ? dst_vect[unroll] : &sink;
          }
          cloned->replaceTargetWith(&tgt, to);
        }
      }
    }

    // cache of introduced phis
    map<pair<const BasicBlock*, const Value*>, Phi*> new_phis;
    unsigned phi_counter = 0;

    topSort();
    DomTree dom_tree(*this, CFG(*this));

    auto bb_of = [&](const Value *val) {
      for (auto *bb : loop_bbs) {
        for (auto &instr : bb->instrs()) {
          if (val == &instr)
            return bb;
        }
      }
      UNREACHABLE();
    };

    // patch users outside of the loop
    for (auto &[val, copies] : vmap) {
      auto I = users.find(val);
      if (I == users.end())
        continue;

      BasicBlock *bb_val = bb_of(val);

      for (auto &[user, user_bb] : I->second) {
        // users inside the loop have been patched already
        if (bbmap.count(user_bb))
          continue;

        // insert a new phi on each dominator exit
        set<pair<BasicBlock*, Phi*>> added_phis;
        Phi *first_added_phi = nullptr;

        for (auto &[exit, dst] : exit_edges) {
          if (!dom_tree.dominates(bb_val, exit))
            continue;

          if (auto phi = dynamic_cast<Phi*>(user);
              phi && user_bb == dst) {
            // Check if the phi uses this value through this predecessor
            auto &vals = phi->getValues();
            auto *used_val = val;
            auto *ex = exit;
            if (!any_of(vals.begin(), vals.end(),
                        [&](const auto &p) {
                          return p.first == used_val && &getBB(p.second) == ex;
                        }))
              continue;

            auto &exit_copies = bbmap.at(exit);
            auto exit_I = exit_copies.begin(), exit_E = exit_copies.end();
            for (auto &[bb, val] : copies) {
              if (++exit_I == exit_E)
                break;
              phi->addValue(*val, string((*exit_I)->getName()));
            }
            continue;
          }

          auto &newphi = new_phis[make_pair(dst, val)];
          if (!newphi) {
            auto name = val->getName() + "#phi#" + to_string(phi_counter++);
            auto phi = make_unique<Phi>(val->getType(), std::move(name));
            newphi = phi.get();
            dst->addInstr(std::move(phi), true);
          }

          // we may have multiple edges from the loop into this BB
          // we need to duplicate the predecessor list for each of the
          // original incoming BB
          auto all_preds = newphi->sources();
          if (find(all_preds.begin(), all_preds.end(), exit->getName()) ==
                all_preds.end()) {
            newphi->addValue(*const_cast<Value*>(val), string(exit->getName()));
            auto &bb_dups = bbmap.at(exit);
            auto bb_I = bb_dups.begin(), bb_E = bb_dups.end();
            for (auto &[_, val] : copies) {
              if (++bb_I == bb_E)
                break;
              newphi->addValue(*val, string((*bb_I)->getName()));
            }
          }

          auto *i = dynamic_cast<Instr*>(user);
          assert(i);
          if (auto phi = dynamic_cast<Phi*>(i)) {
            for (auto &[pv, pred] : phi->getValues()) {
              if (pv == val && dom_tree.dominates(dst, &getBB(pred))){
                phi->replace(pred, *newphi);
              }
            }
          } else {
            i->rauw(*val, *newphi);
            added_phis.emplace(dst, newphi);
            if (!first_added_phi)
              first_added_phi = newphi;
          }
        }

        // We have more than 1 dominating exit
        // add load/stores to avoid complex SSA building algorithms
        if (added_phis.size() > 1) {
          static PtrType ptr_type(0);
          static IntType i32(string("i32"), 32);
          auto &type = val->getType();
          auto size_alloc
            = make_unique<IntConst>(i32, Memory::getStoreByteSize(type));
          auto *size = size_alloc.get();
          addConstant(std::move(size_alloc));

          unsigned align = 16;
          auto name = val->getName() + "#ptr#" + to_string(phi_counter++);
          auto alloca = make_unique<Alloc>(ptr_type, string(name), *size,
                                           nullptr, align);

          auto store = [&](auto *bb, const auto *val) {
            bb->addInstrAt(make_unique<Store>(*alloca.get(),
                                              *const_cast<Value*>(val), align),
                           static_cast<const Instr*>(val), false);
          };

          store(bb_val, val);
          for (auto &[bb, val] : copies) {
            store(bb, val);
          }

          auto load
            = make_unique<Load>(type, name + "#load", *alloca.get(), align);
          auto *i = static_cast<Instr*>(user);
          i->rauw(*first_added_phi, *load.get());
          user_bb->addInstrAt(std::move(load), i, true);

          getFirstBB().addInstr(std::move(alloca), true);
        }
      }
    }

    // patch phis outside the loop with constant values
    for (auto *bb : own_loop_bbs) {
      auto I = phi_preds.find(bb->getName());
      if (I == phi_preds.end())
        continue;

      for (auto &[phi, val] : I->second) {
        if (!vmap.count(phi) && !vmap.count(val)) {
          for (auto *dup : bbmap.at(bb)) {
            // already in the phi
            if (dup == bb)
              continue;
            phi->addValue(*val, string(dup->getName()));
          }
        }
      }
    }
  }
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
    os << "define " << getType() << " @" << name << '(';
    bool first = true;
    for (auto &input : getInputs()) {
      if (!first)
        os << ", ";
      os << input;
      first = false;
    }
    if (isVarArgs())
      os << (first ? "..." : ", ...");
    os << ')' << attrs << " {\n";
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

    if (!(*bbi)->empty()) {
      if (auto instr = dynamic_cast<JumpInstr*>(&(*bbi)->back())) {
        ti = instr->targets().begin();
        te = instr->targets().end();
        return;
      }
    }
    ++bbi;
  }
}

CFG::edge_iterator::edge_iterator(vector<BasicBlock*>::iterator &&it,
                                  vector<BasicBlock*>::iterator &&end)
  : bbi(std::move(it)), bbe(std::move(end)) {
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
    os << '"' << bb_dot_name(src.getName()) << "\" -> \""
       << bb_dot_name(dst.getName()) << "\";\n";
  }
  os << "}\n";
}


// Relies on Alive's top_sort run during llvm2alive conversion in order to
// traverse the cfg in reverse postorder to build dominators.
void DomTree::buildDominators(const CFG &cfg) {
  // initialization
  unsigned i = f.getBBs().size() + 1;
  for (auto &b : f.getBBs()) {
    doms.emplace(b, *b).first->second.order = --i;
  }
  doms.emplace(&f.getSinkBB(), f.getSinkBB()).first->second.order = 0;

  // build predecessors relationship
  for (auto [src, tgt, instr] : cfg) {
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

bool DomTree::dominates(const BasicBlock *a, const BasicBlock *b) const {
  auto *dom_a = &doms.at(a);
  auto *dom_b = &doms.at(b);
  // walk up the dominator tree of 'b' until we find 'a' or the function's entry
  while (true) {
    if (dom_b == dom_a)
      return true;
    if (dom_b == dom_b->dominator)
      break;
    dom_b = dom_b->dominator;
  }
  return false;
}

void DomTree::printDot(ostream &os) const {
  os << "digraph {\n"
        "\"" << bb_dot_name(f.getFirstBB().getName()) << "\" [shape=box];\n";

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
      last[number[bb]] = current - 1;
      worklist.pop_back();
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
      ENSURE(uf.merge(x, w) == w);
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

