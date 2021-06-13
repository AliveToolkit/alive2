#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/expr.h"

namespace IR {

smt::expr float_refined(const smt::expr &a, const smt::expr &b);

}
