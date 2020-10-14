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
* Translation validation plugins for clang and LLVM's `opt`
* Standalone translation validation tool: `alive-tv` ([online](https://alive2.llvm.org))


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
the ``-DZ3_INCLUDE_DIR=/path/to/z3/include`` and ``-DZ3_LIBRARIES=/path/to/z3/lib/libz3.so`` arguments to CMake.


Building and Running Translation Validation
--------

Alive2's `opt` and `clang` translation validation requires a build of LLVM with
RTTI and exceptions turned on.
LLVM can be built in the following way:
```
cd llvm
mkdir build
cmake -GNinja -DLLVM_ENABLE_RTTI=ON -DLLVM_ENABLE_EH=ON -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release -DLLVM_TARGETS_TO_BUILD=X86 -DLLVM_ENABLE_ASSERTIONS=ON -DLLVM_ENABLE_PROJECTS="llvm;clang" ../llvm
```

Alive2 should then be configured as follows:
```
cmake -GNinja -DCMAKE_PREFIX_PATH=~/llvm/build -DBUILD_TV=1 -DCMAKE_BUILD_TYPE=Release ..
```

If you want to use Alive2 as a clang plugin, add `-DCLANG_PLUGIN=1` to the
cmake command.


Translation validation of one or more LLVM passes transforming an IR file on Linux:
```
~/llvm/build/bin/opt -load /home/user/alive2/build/tv/tv.so -tv -instcombine -tv -o /dev/null foo.ll
```
On a Mac:
```
~/llvm/build/bin/opt -load /home/user/alive2/build/tv/tv.dylib -tv -instcombine -tv -o /dev/null foo.ll
```
You can run any pass or combination of passes, but on the command line
they must be placed in between the two invocations of the Alive2 `-tv`
pass.


Translation validation of a single LLVM unit test, using lit:
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


Running the Standalone Translation Validation Tool (alive-tv)
--------

This tool has two modes.

In the first mode, specify a source (original) and target (optimized)
IR file. For example, let's prove that removing `nsw` is correct
for addition:

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

Flipping the inputs yields a counterexample, since it's not correct, in general,
to add `nsw`.
If you are not interested in counterexamples using `undef`, you can use the
command-line argument `-disable-undef-input`.

In the second mode, specify a single unoptimized IR file. alive-tv
will optimize it using an optimization pipeline similar to -O2, but
without any interprocedural passes, and then attempt to validate the
translation.

For example, as of February 6 2020, the `release/10.x` branch contains
an optimizer bug that can be triggered as follows:

```
$ cat foo.ll
define i3 @foo(i3) {
  %x1 = sub i3 0, %0
  %x2 = icmp ne i3 %0, 0
  %x3 = zext i1 %x2 to i3
  %x4 = lshr i3 %x1, %x3
  %x5 = lshr i3 %x4, %x3
  ret i3 %x5
}
$ ./alive-tv foo.ll

----------------------------------------
define i3 @foo(i3 %0) {
  %x1 = sub i3 0, %0
  %x2 = icmp ne i3 %0, 0
  %x3 = zext i1 %x2 to i3
  %x4 = lshr i3 %x1, %x3
  %x5 = lshr i3 %x4, %x3
  ret i3 %x5
}
=>
define i3 @foo(i3 %0) {
  %x1 = sub i3 0, %0
  ret i3 %x1
}
Transformation doesn't verify!
ERROR: Value mismatch

Example:
i3 %0 = #x5 (5, -3)

Source:
i3 %x1 = #x3 (3)
i1 %x2 = #x1 (1)
i3 %x3 = #x1 (1)
i3 %x4 = #x1 (1)
i3 %x5 = #x0 (0)

Target:
i3 %x1 = #x3 (3)
Source value: #x0 (0)
Target value: #x3 (3)

Summary:
  0 correct transformations
  1 incorrect transformations
  0 errors
```

Running the Standalone LLVM Execution Tool (alive-exec)
--------

This tool uses Alive2 as an interpreter for an LLVM function. It is
currently highly experimental and has many restrictions. For example,
the function cannot take inputs, cannot use memory, cannot depend on
undefined behaviors, and cannot include loops that execute too many
iterations.

LLVM Bugs Found by Alive2
--------

[BugList.md](BugList.md) shows the list of LLVM bugs found by Alive2.
