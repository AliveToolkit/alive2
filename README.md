Alive2
======

![Alive2 logo](imgs/alive2.png)

Alive2 consists of several libraries and tools for analysis and verification
of LLVM code and transformations.
Alive2 includes the following libraries:
* Alive2 IR
* Symbolic executor
* LLVM → Alive2 IR converter
* Refinement check (aka optimization verifier)
* SMT abstraction layer

Included tools:
* Alive drop-in replacement
* Translation validation plugins for clang and LLVM's `opt`
* Standalone translation validation tool: `alive-tv` ([online](https://alive2.llvm.org))
* Clang drop-in replacement with translation validation (`alivecc` and
  `alive++`)
* An LLVM IR interpreter that is UB precise (`alive-exec`)

For a technical introduction to Alive2, please see [our paper from
PLDI 2021](https://web.ist.utl.pt/nuno.lopes/pubs/alive2-pldi21.pdf).


WARNING
-------
Alive2 does not support inter-procedural transformations. Alive2 may produce
spurious counterexamples if run with such passes.


Sponsors
--------
We thank the continuous support of all of our sponsors! Alive2 wouldn't be possible without their support.

[![Google](imgs/google.svg)](https://research.google)
&nbsp;&nbsp;&nbsp;&nbsp;
[![NLNet](imgs/nlnet.svg)](https://nlnet.nl)
&nbsp;&nbsp;&nbsp;&nbsp;
[![Woven by Toyota](imgs/woven.svg)](https://woven.toyota)
&nbsp;&nbsp;&nbsp;&nbsp;
[![Matter Labs](imgs/matterlabs.svg)](https://matter-labs.io)

If your company has benefitted from Alive2 (including having a less buggy LLVM), please consider sponsoring our research lab.


Prerequisites
-------------
To build Alive2 you need recent versions of:
* [cmake](https://cmake.org)
* [gcc](https://gcc.gnu.org)/[clang](https://clang.llvm.org)
* [re2c](https://re2c.org/)
* [Z3](https://github.com/Z3Prover/z3)
* [LLVM](https://github.com/llvm/llvm-project) (optional)
* [hiredis](https://github.com/redis/hiredis) (optional, needed for caching)


Building
--------

```
export ALIVE2_HOME=$PWD
export LLVM2_HOME=$PWD/llvm-project
export LLVM2_BUILD=$LLVM2_HOME/build
git clone git@github.com:AliveToolkit/alive2.git
cd alive2
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
RTTI and exceptions turned on. The latest version of Alive2 is always intended
to be built against the latest version of LLVM, using the main branch from
the LLVM repo on Github.
LLVM can be built in the following way.
* You may prefer to add `-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++` to the CMake step if your default compiler is `gcc`.
* Explicitly setting the target may not be necessary.
* `BUILD_SHARED_LIBS` may not be necessary, and for LLVM forks not normally
built with the option, may interfere with CMake files’ use of `USEDLIBS` and
`LLVMLIBS`, and perhaps `dd_llvm_target`.
* To build with Xcode rather than Ninja, replace `-GNinja` with `-GXcode` in
the `cmake` step below, and append `-DLLVM_MAIN_SRC_DIR=$LLVM2_HOME/llvm`. 
  * It may be necessary to disable warnings for “Implicit Conversion to 32 Bit
  Type” in the project build settings.
  * Xcode may place `tv.dylib` in a different location; a symbolic link from the
actual location to that in the resultant error message may help.

```
cd $LLVM2_HOME
mkdir build
cd build
cmake -GNinja -DLLVM_ENABLE_RTTI=ON -DLLVM_ENABLE_EH=ON -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_ASSERTIONS=ON -DLLVM_ENABLE_PROJECTS="llvm;clang" ../llvm
ninja
```

Alive2 should then be configured and built as follows:
```
cd $ALIVE2_HOME/alive2/build
cmake -GNinja -DCMAKE_PREFIX_PATH=$LLVM2_BUILD -DBUILD_TV=1 -DCMAKE_BUILD_TYPE=Release ..
ninja
```

Translation validation of one or more LLVM passes transforming an IR file on Linux:
```
$LLVM2_BUILD/bin/opt -load $ALIVE2_HOME/alive2/build/tv/tv.so -load-pass-plugin $ALIVE2_HOME/alive2/build/tv/tv.so -tv -instcombine -tv -o /dev/null foo.ll
```
For the new pass manager:
```
$LLVM2_BUILD/bin/opt -load $ALIVE2_HOME/alive2/build/tv/tv.so -load-pass-plugin $ALIVE2_HOME/alive2/build/tv/tv.so -passes=tv -passes=instcombine -passes=tv -o /dev/null $LLVM2_HOME/llvm/test/Analysis/AssumptionCache/basic.ll
```


On a Mac with the old pass manager:
```
$LLVM2_BUILD/bin/opt -load $ALIVE2_HOME/alive2/build/tv/tv.dylib -load-pass-plugin $ALIVE2_HOME/alive2/build/tv/tv.dylib -tv -instcombine -tv -o /dev/null foo.ll
```
On a Mac with the new pass manager:
```
$LLVM2_BUILD/bin/opt -load $ALIVE2_HOME/alive2/build/tv/tv.dylib -load-pass-plugin $ALIVE2_HOME/alive2/build/tv/tv.dylib -passes=tv -passes=instcombine -passes=tv -o /dev/null $LLVM2_HOME/llvm/test/Analysis/AssumptionCache/basic.ll
```
You can run any pass or combination of passes, but on the command line
they must be placed in between the two invocations of the Alive2 `-tv`
pass.


Translation validation of a single LLVM unit test, using lit:
```
$LLVM2_BUILD/bin/llvm-lit -vv -Dopt=$ALIVE2_HOME/alive2/build/opt-alive.sh $LLVM2_HOME/llvm/test/Transforms/InstCombine/canonicalize-constant-low-bit-mask-and-icmp-sge-to-icmp-sle.ll
```

The output should be:
```
-- Testing: 1 tests, 1 threads --
PASS: LLVM :: Transforms/InstCombine/canonicalize-constant-low-bit-mask-and-icmp-sge-to-icmp-sle.ll (1 of 1)
Testing Time: 0.11s
  Expected Passes    : 1
```

To run translation validation on all the LLVM unit tests for IR-level
transformations:

```
$LLVM2_BUILD/bin/llvm-lit -s -Dopt=$ALIVE2_HOME/alive2/build/opt-alive.sh $LLVM2_HOME/llvm/test/Transforms
```

We run this command on the main LLVM branch each day, and keep track of the results
[here](https://web.ist.utl.pt/nuno.lopes/alive2/).  To detect unsound transformations in a local run:

```
fgrep -r "(unsound)" $ALIVE2_HOME/alive2/build/logs/
```


Running Alive2 as a Clang Plugin
--------------------------------

This plugin tries to validate every IR-level transformation performed
by LLVM.  Invoke the plugin like this:

```
clang -O3 $LLVM2_HOME/clang/test/C/C99/n505.c -S -emit-llvm \
  -fpass-plugin=$ALIVE2_HOME/alive2/build/tv/tv.so \
  -Xclang -load -Xclang $ALIVE2_HOME/alive2/build/tv/tv.so
```

Or, more conveniently:

```
$ALIVE2_HOME/alive2/build/alivecc -O3 -c $LLVM2_HOME/clang/test/C/C99/n505.c

$ALIVE2_HOME/alive2/build/alive++ -O3 -c $LLVM2_HOME/clang/test/Analysis/aggrinit-cfg-output.cpp
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

In the first mode, specify either a source (original) and target (optimized) IR
file, or a single file containing a function called “src” and also a function
called “tgt”. For example, let’s prove that removing `nsw` is correct for
addition:

```
$ALIVE2_HOME/alive2/build/alive-tv src.ll tgt.ll

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
cat foo.ll

define i3 @foo(i3) {
  %x1 = sub i3 0, %0
  %x2 = icmp ne i3 %0, 0
  %x3 = zext i1 %x2 to i3
  %x4 = lshr i3 %x1, %x3
  %x5 = lshr i3 %x4, %x3
  ret i3 %x5
}

$ALIVE2_HOME/alive2/build/alive-tv foo.ll

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
-------------------------------------------------------

This tool uses Alive2 as an interpreter for an LLVM function. It is
currently highly experimental and has many restrictions. For example,
the function cannot take inputs, cannot use memory, cannot depend on
undefined behaviors, and cannot include loops that execute too many
iterations.

Caching
--------

The alive-tv tool and the Alive2 translation validation opt plugin
support using an external Redis server to avoid performing redundant
queries. This feature is not intended for general use, but rather to
speed up certain systematic testing workloads that perform a lot of
repeated work. When it hits a repeated refinement check, it prints
"Skipping repeated query" instead of performing the query.

If you want to use this functionality, you will need to manually start
and stop, as appropriate, a Redis server instance on localhost. Alive2
should be the only user of this server.

Diagnosing Unsoundness Reports
------------------------------

* Select a failing test file. It may be convenient to choose one whose path is
given at the beginning of a log file containing the text "(unsound)" as above;
this is guaranteed to contain an unsoundness report.  Many log files, however,
contain only “Source: \<stdin\>” rather than a file path; the names of these
files begin with “in_”.
* Do a verbose run of Lit for just that file, with the `opt`  option
`--print-after-all` appended.  (You may also append other `opt`  options, such
as other optimizations.)  E.g.:
```
$LLVM2_BUILD/bin/llvm-lit -vva "-Dopt=$ALIVE2_HOME/alive2/build/opt-alive.sh --print-after-all" $LLVM2_HOME/llvm/test/Transforms/InstCombine/insert-const-shuf.ll
```
* Collect Lit’s LLVM IR terminal output, for comparison with Alive2’s Alive2 IR
output in the log file indicated by “Report written to…”.  Sometimes the Lit
output may not contain useful LLVM IR, in which case executing the output
RUN command separately may give better results.
* The Alive2 unsoundness report in the corresponding log file will have two
versions of the misoptimized function.  The Alive2 IR function body may
indicate the problem to a human, but for Alive2 translation validation
you will need LLVM IR.  Search for the function name in the terminal output.
* Copy the first function definition and necessary declarations and metadata to
either a new file or to the Alive2 Compiler Explorer instance,
[https://alive2.llvm.org/ce/](https://alive2.llvm.org/ce/).
(The `-allow-incomplete-ir` flag may make copying declarations and metadata
unnecessary.)
The Alive2 Compiler Explorer instance will run automatically;
to check with the standalone `alive-tv`, see its instructions above.
Without a second version of the function to compare, Alive2 just runs the
`-O2` optimizations;
if it reports unsoundness, your fork’s optimizations are not to blame.
* If there is a second, unsound, function definition in the LLVM IR terminal
output, copy it and necessary declarations, and change the
second function name.
* If it now reports a misoptimization, presumably your fork has a bug,
demonstrated by the provided examples.
* To screen out exact duplicate reports when comparing different test runs,
move the `logs` directory out of the way before each run.  After each run, copy
the relevant logs to a separate destination directory.  (Systems with a non-GNU
version of `cp` will need to use coreutils’ `gcp` instead.)
```
fgrep --files-with-matches --recursive "(unsound)" $ALIVE2_HOME/alive2/build/logs/ |  xargs cp -p --target-directory=<Destination>

```
* Unique unsoundness reports can then be found with a utility such as `jdupes --print-unique`.  
  * If the tests are run on different LLVM directories, the “Source:” line in
  files whose name does not begin with “in_”, as well as “Command line:” lines
  on Linux, should be stripped before comparison.


Troubleshooting
---------------
* Check the “LLVMConfig.cmake” and “CMAKE_PREFIX_PATH” output from CMake in
case of build problems. CMake may look for configuration information in old
installations of LLVM, e.g., under `/opt/`, if these are not set properly.
* Some combinations of Clang and MacOS versions may give link warnings 
“-undefined dynamic_lookup may not work with chained fixups,” and
runtime errors with “symbol not found in flat namespace.”  Setting
[CMAKE_OSX_DEPLOYMENT_TARGET](https://cmake.org/cmake/help/latest/variable/CMAKE_OSX_DEPLOYMENT_TARGET.html)
as a cache entry to 11.0 or less at the beginning of CMakeLists.txt may work
around this.
* Building for Translation Validation is tightly coupled to LLVM top of tree
source.  Building a fork with older source may require reverting to the
corresponding Alive2 commit.  This in turn may require experimentation with
Clang and SDK versions and vendors.
* Building older source on an up-to-date machine may require adjustments.  For
example, the now-deleted file `scripts/rewritepass.py` depended on the
deprecated Python 2; update the shebang line to `python3`.
* The `opt` wrapper script `build/opt-alive.sh` accepts a `--verbose` option,
which outputs the command passed to `opt`.  Note that this may interfere 
with tests which check output.
* The script also accepts a `--no-timeout` option, which disables the `opt`
process timeout.  This timeout is not supported on Macintosh.  To change the
SMT timeout, instead pass an `-smt-to:` option to the `alive` executable.

LLVM Bugs Found by Alive2
-------------------------

[BugList.md](BugList.md) shows the list of LLVM bugs found by Alive2.
