## Category 1
This category is for bugs that generating wrong codes from passes
1. miscompile due to instcombine(https://github.com/llvm/llvm-project/issues/53252)
2. Another miscompile due to instcombine(https://github.com/llvm/llvm-project/issues/50693)
3. NewGVN miscompiles(https://github.com/llvm/llvm-project/issues/53218)
4. arm64 backend bug (https://github.com/llvm/llvm-project/issues/55003 && https://github.com/llvm/llvm-project/issues/55178)
5. arm64 miscompile (https://github.com/llvm/llvm-project/issues/55201)
6. arm64 miscompile due to global isel (https://github.com/llvm/llvm-project/issues/55129)
7. arm64, x86, and x86-64 miscompile from undef and urem (https://github.com/llvm/llvm-project/issues/55271)
8. or+and miscompile with global isel on arm64 (https://github.com/llvm/llvm-project/issues/55284)
9. [Open] urem+udiv miscompile with global isel on arm64(https://github.com/llvm/llvm-project/issues/55287)
10. fshl-related miscompile by arm64 and x86-64 backends(https://github.com/llvm/llvm-project/issues/55296)
11. arm64 miscompile (https://github.com/llvm/llvm-project/issues/55342)
12. miscompile from multiple backends (https://github.com/llvm/llvm-project/issues/55484)
13. miscompile from arm64 backend with (icmp ult (sub -6, -8) 3) (https://github.com/llvm/llvm-project/issues/55490)
14. shift/zext-related miscompile by aarch64 backend (https://github.com/llvm/llvm-project/issues/55833)

## Category 2
This category is for bugs that crashes
1. Instcombine crashes(https://github.com/llvm/llvm-project/issues/52884)
2. newGVN crashes (https://bugs.llvm.org/show_bug.cgi?id=51618)

## Category 3
This category is for that the test file is originaly wrong
1. Wrong test file in Coroutines(https://github.com/llvm/llvm-project/commit/ea6a3f9f960e52ea39edd5edddf5afad3c11f7a0)
