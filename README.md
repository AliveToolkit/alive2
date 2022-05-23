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

For a technical introduction to Alive2, please see [our paper from
PLDI 2021](https://web.ist.utl.pt/nuno.lopes/pubs.php?id=alive2-pldi21).


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
cd build
cmake -GNinja -DLLVM_ENABLE_RTTI=ON -DLLVM_ENABLE_EH=ON -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release -DLLVM_TARGETS_TO_BUILD=X86 -DLLVM_ENABLE_ASSERTIONS=ON -DLLVM_ENABLE_PROJECTS="llvm;clang" ../llvm
```

Alive2 should then be configured as follows:
```
cmake -GNinja -DCMAKE_PREFIX_PATH=~/llvm/build -DBUILD_TV=1 -DCMAKE_BUILD_TYPE=Release ..
```

Translation validation of one or more LLVM passes transforming an IR file on Linux:
```
~/llvm/build/bin/opt -load $HOME/alive2/build/tv/tv.so -load-pass-plugin $HOME/alive2/build/tv/tv.so -tv -instcombine -tv -o /dev/null foo.ll
```
On a Mac:
```
~/llvm/build/bin/opt -load $HOME/alive2/build/tv/tv.dylib -load-pass-plugin $HOME/alive2/build/tv/tv.dylib -tv -instcombine -tv -o /dev/null foo.ll
```
You can run any pass or combination of passes, but on the command line
they must be placed in between the two invocations of the Alive2 `-tv`
pass.


Translation validation of a single LLVM unit test, using lit:
```
~/llvm/build/bin/llvm-lit -vv -Dopt=$HOME/alive2/build/opt-alive.sh ~/llvm/llvm/test/Transforms/InstCombine/canonicalize-constant-low-bit-mask-and-icmp-sge-to-icmp-sle.ll
```

The output should be:
```
-- Testing: 1 tests, 1 threads --
PASS: LLVM :: Transforms/InstCombine/canonicalize-constant-low-bit-mask-and-icmp-sge-to-icmp-sle.ll (1 of 1)
Testing Time: 0.11s
  Expected Passes    : 1
```

To run translation validation on all of the LLVM unit tests for
IR-level transformations:

```
~/llvm/build/bin/llvm-lit -vv -Dopt=$HOME/alive2/build/opt-alive.sh ~/llvm/llvm/test/Transforms
```

We run this command on the main LLVM branch each day, and keep track of the results
[here](https://web.ist.utl.pt/nuno.lopes/alive2/).


Running Alive2 as a Clang Plugin
--------

This plugin tries to validate every IR-level transformation performed
by LLVM.  Invoke the plugin like this:

```
$ clang -O3 <src.c> -S -emit-llvm \
  -fpass-plugin=$HOME/alive2/build/tv/tv.so \
  -Xclang -load -Xclang $HOME/alive2/build/tv/tv.so
```

Or, more conveniently:

```
$ $HOME/alive2/build/alivecc -O3 -c <src.c>
$ $HOME/alive2/build/alive++ -O3 -c <src.cpp>
```

The Clang plugin can optionally use multiple cores. To enable parallel
translation validation, add the `-mllvm -tv-parallel=XXX` command line
options to Clang, where XXX is one of two parallelism managers
supported by Alive2. The first (XXX=fifo) uses alive-jobserver: for
details about how to use this program, please consult its help output
by running it without any command line arguments. The second
parallelism manager (XXX=unrestricted) does not restrict parallelism
at all, but rather calls fork() freely. This is mainly intended for
developer use; it tends to use a lot of RAM.

Use the `-mllvm -tv-report-dir=dir` to tell Alive2 to place its output
files into a specific directory.

The Clang plugin's output can be voluminous. To help control this, it
supports an option to reduce the amount of output (`-mllvm
-tv-quiet`).

Our goal is for the `alivecc` and `alive++` compiler drivers to be
drop-in replacements for `clang` and `clang++`. So, for example, they
try to detect when they are being invoked as assemblers or linkers, in
which case they do not load the Alive2 plugin. This means that some
projects cannot be built if you manually specify command line options
to Alive2, for example using `-DCMAKE_C_FLAGS=...`. Instead, you can
tell `alivecc` and `alive++` what to do using a collection of
environment variables that generally mirror the plugin's command line
interface. For example:

```
ALIVECC_PARALLEL_UNRESTRICTED=1
ALIVECC_PARALLEL_FIFO=1
ALIVECC_DISABLE_UNDEF_INPUT=1
ALIVECC_DISABLE_POISON_INPUT=1
ALIVECC_SMT_TO=timeout in milliseconds
ALIVECC_SUBPROCESS_TIMEOUT=timeout in seconds
ALIVECC_OVERWRITE_REPORTS=1
ALIVECC_REPORT_DIR=dir
```

If validating the program takes a long time, you can batch optimizations to
verify.
Please set `ALIVECC_BATCH_OPTS=1` and run `alivecc`/`alive++`.


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

Please keep in mind that you do not have to compile Alive2 in order to
try out alive-tv; it is available online: https://alive2.llvm.org/ce/


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
