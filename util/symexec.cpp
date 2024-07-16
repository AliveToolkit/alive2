// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "util/symexec.h"
#include "ir/function.h"
#include "ir/state.h"
#include "util/config.h"
#include <iostream>

using namespace IR;
using namespace std;
using util::config::dbg;

static void sym_exec_instr(State &s, const Instr &i) {
  auto &val = s.exec(i);

  if (util::config::symexec_print_each_value) {
    auto &name = i.getName();
    dbg() << name;
    if (name[0] == '%')
      dbg() << " = " << val.val << " /";
    dbg() << " UB=" << val.domain << '\n';
    if (!val.return_domain.isTrue())
      dbg() << " RET=" << val.return_domain  << '\n';
  }
}

namespace util {

void sym_exec_init(State &s) {
  Function &f = const_cast<Function&>(s.getFn());

  // global constants need to be created in the right order so they get the
  // first bids in source, and the last in target
  set<const Value*> seen_inits;
  for (const auto &v : f.getConstants()) {
    if (auto gv = dynamic_cast<const GlobalVariable*>(&v)) {
      if (gv->isConst() == s.isSource()) {
        s.exec(v);
        seen_inits.emplace(&v);
      }
    }
  }

  // add constants & inputs to State table first of all
  for (auto &l : { f.getConstants(), f.getInputs(), f.getUndefs() }) {
    for (const auto &v : l) {
      if (!seen_inits.count(&v))
        s.exec(v);
    }
  }

  if (f.getFirstBB().getName() == "#init") {
    for (auto &i : f.getFirstBB().instrs()) {
      sym_exec_instr(s, i);
    }
  }
  s.finishInitializer();

  s.exec(Value::voidVal);
}

void sym_exec(State &s) {
  sym_exec_init(s);

  Function &f = const_cast<Function&>(s.getFn());

  for (auto &bb : f.getBBs()) {
    if (!s.startBB(*bb) || bb->getName() == "#init")
      continue;

    for (auto &i : bb->instrs()) {
      sym_exec_instr(s, i);
    }
  }

  if (config::symexec_print_each_value) {
    auto ret = s.returnVal();
    dbg() << "domain = " << ret.domain
          << "\nreturn domain = " << ret.return_domain
          << "\nreturn = " << ret.val
          << s.returnMemory() << "\n\n";
  }
}

}
