#!/bin/sh

mkdir build
cd build

if [[ "$RUNNER_OS" == "Linux" ]]; then
  LLVM_DIR="/usr/lib/llvm-21"
elif [[ "$RUNNER_OS" == "macOS" ]]; then
  LLVM_DIR="$(brew --prefix llvm)/lib/cmake/llvm"
fi

cmake -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE \
      -DCMAKE_CXX_COMPILER=$CMAKE_CXX_COMPILER \
      -DCMAKE_CXX_FLAGS=$CMAKE_CXX_FLAGS \
      -DCMAKE_PREFIX_PATH="$LLVM_DIR" \
      -DBUILD_TV=1 \
      -GNinja ..
ninja
