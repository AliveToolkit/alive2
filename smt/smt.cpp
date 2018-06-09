// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/smt.h"
#include "smt/ctx.h"
#include <z3.h>

namespace smt {

smt_initializer::smt_initializer() {
  ctx.init();
}

smt_initializer::~smt_initializer() {
  ctx.destroy();
  Z3_finalize_memory();
}

}
