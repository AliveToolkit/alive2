// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "util/symexec.h"
#include "ir/function.h"
#include "ir/state.h"
#include "util/config.h"
#include <iostream>

using namespace IR;
using namespace std;

namespace util {

void sym_exec(State &s) {
  Function &f = const_cast<Function&>(s.getFn());

  // add constants & inputs to State table first of all
  for (auto &l : { f.getConstants(), f.getInputs(), f.getUndefs() }) {
    for (const auto &v : l) {
      s.exec(v);
    }
  }

  s.exec(Value::voidVal);

  bool first = true;
  if (f.getFirstBB().getName() != "#init") {
    s.finishInitializer();
    first = false;
  }

  for (auto &bb : f.getBBs()) {
    if (!s.startBB(*bb))
      continue;

    for (auto &i : bb->instrs()) {
      auto val = s.exec(i);
      auto &name = i.getName();

      if (config::symexec_print_each_value && name[0] == '%')
        cout << name << " = " << val << '\n';
    }

    if (first)
      s.finishInitializer();
    first = false;
  }

  if (config::symexec_print_each_value) {
    cout << "domain = " << s.returnDomain()
         << "\nreturn = " << s.returnVal().first
         << s.returnMemory() << "\n\n";
  }
}

}
