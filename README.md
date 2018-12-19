Alive2
======

WIP; do not use!

Prerequisites
-------------

* Z3 4.8.3
* re2c

Building
--------

```
mkdir build
cd build
cmake ..
make
```

If CMake cannot find the Z3 include directory (or finds the wrong one) pass
the ``-DZ3_INCLUDE_DIR=/path/to/z3/include`` argument to CMake

Building and Running Translation Validation
--------

Build an LLVM after configuring it using the ``-DLLVM_ENABLE_RTTI=ON
-DLLVM_ENABLE_EH=ON`` options to CMake

Copy ``alive2/scripts/opt-alive.sh`` somewhere and edit it to point to
the proper locations, keeping in mind that lit strips environment
variables other than $HOME

```
mkdir build
cd build
cmake .. -GNinja -DBUILD_TV=1 -DCMAKE_BUILD_TYPE=Release
ninja
```

Translation validation of a single test case:

```
${TEST_LLVM_BUILD}/bin/llvm-lit -vv -Dopt="/path/to/opt-alive.sh" ${TEST_LLVM_SRC}/test/Transforms/InstCombine/canonicalize-constant-low-bit-mask-and-icmp-sge-to-icmp-sle.ll
```

The output should be:

```
-- Testing: 1 tests, 1 threads --
PASS: LLVM :: Transforms/InstCombine/canonicalize-constant-low-bit-mask-and-icmp-sge-to-icmp-sle.ll (1 of 1)
Testing Time: 0.11s
  Expected Passes    : 1
```

Translation validation of the LLVM unit tests:

```
${TEST_LLVM_BUILD}/bin/llvm-lit -s -Dopt="/path/to/opt-alive.sh" ${TEST_LLVM_SRC}/test/Transforms
```
