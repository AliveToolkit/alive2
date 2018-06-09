// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "util/symexec.h"
#include "ir/function.h"
#include "ir/state.h"
#include "util/config.h"
#include <iostream>

using namespace IR;
using namespace util;
using namespace std;

namespace util {

void sym_exec(State &s) {
  Function &f = const_cast<Function&>(s.getFn());

  // add constants & inputs to State table first of all
  for (auto &c : f.getConstants()) {
    s.add(c, c.toSMT(s));
  }

  for (auto &i : f.getInputs()) {
    s.add(i, i.toSMT(s));
  }

  for (auto &bb : f.getBBs()) {
    if (!s.startBB(*bb))
      continue;

    for (auto &i : bb->instrs()) {
      auto val = i.toSMT(s);
      if (!i.getName().empty()) {
        if (config::symexec_print_each_value)
          cout << i.getName() << " = " << val << '\n';
        s.add(i, move(val));
      }
    }
  }
}

}
