#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include <string>

struct redisContext;

class Cache {
  redisContext *ctx = nullptr;

public:
  Cache(unsigned port, bool allow_version_mismatch);
#ifndef NO_REDIS_SUPPORT
  ~Cache();
#endif
  bool lookup(const std::string &s);
};
