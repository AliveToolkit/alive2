#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

typedef struct _Z3_context *Z3_context;
typedef struct _Z3_params* Z3_params;

namespace smt {

class context {
  Z3_context ctx;
  Z3_params no_timeout_param;

public:
  Z3_context operator()() const { return ctx; }
  Z3_params getNoTimeoutParam() const { return no_timeout_param; }

  void init();
  void destroy();
};

extern context ctx;

}
