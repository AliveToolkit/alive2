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
9. urem+udiv miscompile with global isel on arm64(https://github.com/llvm/llvm-project/issues/55287)
10. fshl-related miscompile by arm64 and x86-64 backends(https://github.com/llvm/llvm-project/issues/55296)
11. arm64 miscompile (https://github.com/llvm/llvm-project/issues/55342)
12. miscompile from multiple backends (https://github.com/llvm/llvm-project/issues/55484)
13. miscompile from arm64 backend with (icmp ult (sub -6, -8) 3) (https://github.com/llvm/llvm-project/issues/55490)
14. miscompile of non-canonical add+icmp by arm64 backend (https://github.com/llvm/llvm-project/issues/55627)
15. shift/zext-related miscompile by aarch64 backend (https://github.com/llvm/llvm-project/issues/55833)
16. possible zext-related miscompile with global isel on AArch64 (https://github.com/llvm/llvm-project/issues/56733)
17. miscompile a usub.sat on AArch64 (https://github.com/llvm/llvm-project/issues/58109)
18. miscompile of a frozen poison by AArch64 backend (https://github.com/llvm/llvm-project/issues/58321)
19. miscompile from aarch64 backend with global isel (https://github.com/llvm/llvm-project/issues/58431)
20. i34 miscompile in miscombine (https://github.com/llvm/llvm-project/issues/59836)


## Category 2
This category is for bugs that crashes
1. Instcombine crashes(https://github.com/llvm/llvm-project/issues/52884)
2. newGVN crashes (https://bugs.llvm.org/show_bug.cgi?id=51618)
3. VectorCombine crashes(https://github.com/llvm/llvm-project/issues/56377)
4. InstCombine Crashes(https://github.com/llvm/llvm-project/issues/56463)
5. InstCombine crashes (https://github.com/llvm/llvm-project/issues/56945)
6. InstSimplify Bug (https://github.com/llvm/llvm-project/issues/56968)
7. InstCombine Crash (https://github.com/llvm/llvm-project/issues/56981)
8. aarch64 global isel backend crashes on valid input (https://github.com/llvm/llvm-project/issues/58423)
9. aarch64 global isel backend assertion violation when translating udiv of i1 (https://github.com/llvm/llvm-project/issues/58425)
10. fprintf() requires two pointer arguments (https://github.com/llvm/llvm-project/issues/59757)
11. opt crash (https://github.com/llvm/llvm-project/issues/57832)

## Category 3
This category is for that the test file is originaly wrong
1. Wrong test file in Coroutines(https://github.com/llvm/llvm-project/issues/52912)
