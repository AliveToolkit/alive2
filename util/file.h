#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include <memory>
#include <string>
#include <string_view>

namespace util {

class file_reader {
  std::unique_ptr<char[]> buf;
  size_t sz;

public:
  file_reader(const char *filename, unsigned padding = 0);

  std::string_view operator*() const {
    return { buf.get(), sz };
  }
};

struct FileIOException {};

std::string get_random_filename(const std::string &dir, const char *extension, const char *prefix = nullptr);

}
