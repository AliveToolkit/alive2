#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include <memory>
#include <string_view>

namespace util {

class file_reader {
  char *buf = nullptr;
  size_t sz;

public:
  file_reader(const char *filename, unsigned padding = 0);
  ~file_reader();

  std::string_view operator*() const {
    return { buf, sz };
  }
};

struct FileIOException {};

}
