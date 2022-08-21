#pragma once

#include <hiredis/hiredis.h>
#include <memory>
#include <string>
#include <string_view>

class Cache {
  redisContext *ctx = nullptr;

public:
  Cache(unsigned port, bool allow_version_mismatch);
  ~Cache();
  bool lookup(const std::string &s);
};
