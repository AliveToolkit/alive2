Alive2
======

Alive2 consists of several libraries and tools for analysis and verification
of LLVM code and transformations.
Alive2 includes the following libraries:
* Alive2 IR
* Symbolic executor
* LLVM -> Alive2 IR converter
* Refinement check (aka optimization verifier)
* SMT abstraction layer

Included tools:
* Alive drop-in replacement
* Translation validation plugin for LLVM's `opt`


Prerequisites
-------------
To build Alive2 you need recent versions of:
* cmake
* gcc/clang
* re2c
* Z3
* LLVM (optional)


Building
--------

```
mkdir build
cd build
cmake -GNinja -DCMAKE_BUILD_TYPE=Release ..
ninja
```

If CMake cannot find the Z3 include directory (or finds the wrong one) pass
the ``-DZ3_INCLUDE_DIR=/path/to/z3/include`` argument to CMake


Building and Running Translation Validation
--------

Alive2's `opt` translation validation requires a build of LLVM with RTTI and
exceptions turned on.
LLVM can be built in the following way:
```
cd llvm
mkdir build
cmake -GNinja -DLLVM_ENABLE_RTTI=ON -DLLVM_ENABLE_EH=ON -DCMAKE_BUILD_TYPE=Release -DLLVM_TARGETS_TO_BUILD=X86 -DLLVM_ENABLE_ASSERTIONS=ON -DLLVM_ENABLE_PROJECTS="llvm;clang" ../llvm
```

Alive2 should then be configured as follows:
```
cmake -GNinja -DLLVM_DIR=~/llvm/build/lib/cmake/llvm -DBUILD_TV=1 -DCMAKE_BUILD_TYPE=Release ..
```


Translation validation of a single test case:
```
~/llvm/build/bin/llvm-lit -vv -Dopt=/home/user/alive2/scripts/opt-alive.sh ~/llvm/llvm/test/Transforms/InstCombine/canonicalize-constant-low-bit-mask-and-icmp-sge-to-icmp-sle.ll
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
~/llvm/build/bin/llvm-lit -vv -Dopt=/home/user/alive2/scripts/opt-alive.sh ~/llvm/llvm/test/Transforms
```
