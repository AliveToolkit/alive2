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
* Standalone translation validation tool: `alive-tv`


WARNING
-------
Alive2 does not support inter-procedural transformations. Alive2 may crash
or produce spurious counterexamples if run with such passes.


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

Alive2's `opt` and `clang` translation validation requires a build of LLVM with
RTTI and exceptions turned on.
LLVM can be built in the following way:
```
cd llvm
mkdir build
cmake -GNinja -DLLVM_ENABLE_RTTI=ON -DLLVM_ENABLE_EH=ON -DLLVM_BUILD_LLVM_DYLIB=ON -DCMAKE_BUILD_TYPE=Release -DLLVM_TARGETS_TO_BUILD=X86 -DLLVM_ENABLE_ASSERTIONS=ON -DLLVM_ENABLE_PROJECTS="llvm;clang" ../llvm
```
Note that `-DLLVM_BUILD_LLVM_DYLIB=ON` is optional, `-DBUILD_SHARED_LIBS=ON` works too.

Alive2 should then be configured as follows:
```
cmake -GNinja -DLLVM_DIR=~/llvm/build/lib/cmake/llvm -DBUILD_TV=1 -DCMAKE_BUILD_TYPE=Release ..
```

If you want to use Alive2 as a clang plugin, add `-DCLANG_PLUGIN=1` to the
cmake command.


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

Running Alive2 as a Clang plugin:

```
$ clang -O3 <src.c> -S -emit-llvm \
  -fpass-plugin=$HOME/alive2/build/tv/tv.so -fexperimental-new-pass-manager \
  -Xclang -load -Xclang $HOME/alive2/build/tv/tv.so
```


Running Standalone Translation Validation Tool (alive-tv)
--------

This tool takes two arguments: source and target (optimized) bitcode.
For example, let's prove that removing `nsw` is correct for addition:

```
$ ./alive-tv src.ll tgt.ll

----------------------------------------
define i32 @f(i32 %a, i32 %b) {
  %add = add nsw i32 %b, %a
  ret i32 %add
}
=>
define i32 @f(i32 %a, i32 %b) {
  %add = add i32 %b, %a
  ret i32 %add
}

Transformation seems to be correct!
```

Flipping the inputs yields a counterexample, since it's not correct to
add `nsw` without further information.
If you are not interested in counterexamples using `undef`, you can use the
command-line argument `-disable-undef-input`.
