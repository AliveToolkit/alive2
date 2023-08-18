#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

namespace IR { class State; }

namespace util {

void sym_exec_init(IR::State &s);
void sym_exec(IR::State &s);

}
