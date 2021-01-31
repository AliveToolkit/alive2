// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/init.h"
#include "ir/memory.h"

namespace IR {

void initializer::reset() {
  Memory::cleanGlobals();
}

}
