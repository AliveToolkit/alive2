#!/bin/sh

mkdir build
cd build

if [[ "$RUNNER_OS" == "Linux" ]]; then
  LLVM_VERSION="${CMAKE_CXX_COMPILER##*-}"   # extracts 20 or 21
  LLVM_DIR="usr/lib/llvm-${LLVM_VERSION}/lib/cmake"
elif [[ "$RUNNER_OS" == "macOS" ]]; then
  LLVM_DIR="$(brew --prefix llvm)/lib/cmake/llvm"
fi

cmake -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE \
      -DCMAKE_CXX_COMPILER=$CMAKE_CXX_COMPILER \
      -DCMAKE_CXX_FLAGS=$CMAKE_CXX_FLAGS \
      -DCMAKE_PREFIX_PATH=$LLVM_DIR \
      -DBUILD_TV=1 \
      -GNinja ..
ninja
