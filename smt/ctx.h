#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

typedef struct _Z3_context *Z3_context;

namespace smt {

class context {
  Z3_context ctx;

public:
  Z3_context operator()() const { return ctx; }

  void init();
  void destroy();
};

extern context ctx;

}
