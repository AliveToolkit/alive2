#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

namespace IR {

struct initializer {
  void reset();
  ~initializer() { reset(); }
};

}
