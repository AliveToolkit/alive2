#!/bin/sh

mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE \
      -DCMAKE_CXX_COMPILER=$CMAKE_CXX_COMPILER \
      -DCMAKE_CXX_FLAGS=$CMAKE_CXX_FLAGS \
      -DBUILD_TV=1 \
      -GNinja ..
ninja
