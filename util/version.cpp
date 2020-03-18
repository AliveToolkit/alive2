// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "util/version.h"
#include "version_gen.h"

#define xstr(s) str(s)
#define str(s) #s

namespace util {

// clang-format off
const char alive_version[] = {
  xstr(ALIVE_VERSION_MACRO)
};
// clang-format on

} // namespace util
