#include "cache.h"
#include "crc.h"
#include "util/errors.h"
#include "util/version.h"

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

static string redis_reply_string(int reply_type) {
  switch (reply_type) {
  case REDIS_REPLY_STRING:
    return "STRING";
  case REDIS_REPLY_ARRAY:
    return "ARRAY";
  case REDIS_REPLY_INTEGER:
    return "INTEGER";
  case REDIS_REPLY_NIL:
    return "NIL";
  case REDIS_REPLY_STATUS:
    return "STATUS";
  case REDIS_REPLY_ERROR:
    return "ERROR";
  default:
    return "UNKNOWN REPLY";
  }
}

static bool remote_get(const string &key, string &value, redisContext *ctx) {
  assert(ctx);
  redisReply *reply = (redisReply *)redisCommand(ctx, "GET %s", key.data());
  if (!reply || ctx->err) {
    cerr << "Redis error in remote_get: " << ctx->errstr << "\n";
    exit(-1);
  }
  if (reply->type == REDIS_REPLY_NIL) {
    // not found
    freeReplyObject(reply);
    return false;
  } else if (reply->type == REDIS_REPLY_STRING) {
    // found
    value = reply->str;
    freeReplyObject(reply);
    return true;
  } else {
    cerr << "Redis protocol error in remote_get, didn't expect reply type "
         << redis_reply_string(reply->type) << "\n";
    exit(-1);
  }
}

static void remote_set(const string &key, const string &value, redisContext *ctx) {
  assert(ctx);
  redisReply *reply =
      (redisReply *)redisCommand(ctx, "SET %s %s", key.data(), value.data());
  if (!reply || ctx->err) {
    cerr << "Redis error in remote_set: " << ctx->errstr << "\n";
    exit(-1);
  }
  if (reply->type != REDIS_REPLY_STATUS) {
    cerr << "Redis protocol error in remote_set, didn't expect reply type "
         << redis_reply_string(reply->type) << "\n";
    exit(-1);
  }
  freeReplyObject(reply);
}

static crc_t string_crc(const string &s) {
  return crc_finalize(crc_update(crc_init(), s.data(), s.size()));
}

bool Cache::lookup(const string &s) {
  static const string default_value("XXX");
  // Alive IR is bulky, so send a hash of it over to the cache. If
  // this still uses too much RAM, the next step will be to use a
  // Bloom filter
  string remote_data, crc_string = to_string(string_crc(s));
  if (remote_get(crc_string, remote_data, ctx)) {
    assert(remote_data == default_value);
    return true;
  } else {
    remote_set(crc_string, default_value, ctx);
    return false;
  }
}

Cache::Cache(unsigned port, bool allow_version_mismatch) {
  const char *hostname = "127.0.0.1";
  struct timeval timeout = {1, 500000}; // 1.5 seconds
  ctx = redisConnectWithTimeout(hostname, port, timeout);
  if (!ctx) {
    cerr << "Can't allocate redis context\n";
    exit(-1);
  }
  if (ctx->err) {
    cerr << "Redis connection error: " << ctx->errstr << "\n";
    exit(-1);
  }
  string version;
  if (remote_get("Alive2_version", version, ctx)) {
    if (version != util::alive_version) {
      cerr << "Cache version mismatch!\n";
      cerr << "This version of Alive2 is " << util::alive_version << "\n";
      cerr << "But the cache was created by version " << version << "\n";
      if (!allow_version_mismatch)
        exit(-1);
    }
  } else {
    remote_set("Alive2_version", util::alive_version, ctx);
  }
}


Cache::~Cache() {
  redisFree(ctx);
}
