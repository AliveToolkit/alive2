## The List of LLVM Bugs Found by Alive2

This document enumerates the bugs in LLVM found by Alive2.
This includes bugs found in the past version of Alive2 but not by the recent
version anymore, or bugs that are found by locally modified version of Alive2.
For example, [pr43616](https://bugs.llvm.org/show_bug.cgi?id=43616)
is not found by Alive2 anymore because accepting a pair that converts a
non-constant global variable into constant can introduce false negatives.
Such pair is now reported as unsupported, but it was accepted in the past.


### Memory Operations (Load/Store/GEP/...)

1. EmitGEPOffset() incorrectly adds NUW to multiplications
(https://bugs.llvm.org/show_bug.cgi?id=42699)

2. invalid bitcast->gep inbounds
(https://bugs.llvm.org/show_bug.cgi?id=43501)

3. InstCombine incorrectly shrinks the size of store
(https://bugs.llvm.org/show_bug.cgi?id=44306)

4. InstCombine incorrectly folds 'gep(bitcast ptr), idx' into 'gep ptr, idx'
(https://bugs.llvm.org/show_bug.cgi?id=44321)

5. memcpyopt adds incorrect align to memset
(https://bugs.llvm.org/show_bug.cgi?id=44388)

6. Folding 'gep p, (q - p)' to q should check it is never used for loads & stores
(https://bugs.llvm.org/show_bug.cgi?id=44403)

7. StraightLineStrengthReduce can introduce UB when optimizing 2-dim array gep
(https://bugs.llvm.org/show_bug.cgi?id=44533)

8. SLPVectorizer should drop nsw flags from add
(https://bugs.llvm.org/show_bug.cgi?id=44536)

9. InstCombine should correctly propagate alignment
(https://bugs.llvm.org/show_bug.cgi?id=44543)

10. DSE not checking decl of libcalls
(https://github.com/llvm/llvm-project/commit/87407fc03c82d880cc42330a8e230e7a48174e3c)

11. [globalopt] optimization leaves store to a constant global
(https://bugs.llvm.org/show_bug.cgi?id=43616)

12. -expandmemcmp generates loads with incorrect alignment
(https://bugs.llvm.org/show_bug.cgi?id=43880)

13. InstCombine incorrectly rewrites indices of gep inbounds
(https://bugs.llvm.org/show_bug.cgi?id=44861)