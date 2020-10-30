## Bugs Found by Alive2

This document lists the bugs found so far by Alive2 in LLVM & Z3.
Please contact us or submit a PR if something is missing or inaccurate.


### Integer/Float operations

1. Incorrect fold of 'x & (-1 >> y) s>= x'
(https://bugs.llvm.org/show_bug.cgi?id=39861)

2. Incorrect instcombine fold for icmp sgt
(https://bugs.llvm.org/show_bug.cgi?id=42198)

3. EmitGEPOffset() incorrectly adds NUW to multiplications
(https://bugs.llvm.org/show_bug.cgi?id=42699)

4. Incorrect transformation of minnum with nnan
(https://bugs.llvm.org/show_bug.cgi?id=45478)

5. InstSimplify: fadd (nsz op), +0 incorrectly removed
(https://bugs.llvm.org/show_bug.cgi?id=45778)

6. Incorrect instcombine fold of control-flow to umul.with.overflow
(https://bugs.llvm.org/show_bug.cgi?id=45952)

7. Jumpthreading introduces jump on poison
(https://bugs.llvm.org/show_bug.cgi?id=45956)

8. SCEVExpander introduces branch on poison
(https://bugs.llvm.org/show_bug.cgi?id=47769)

9. Incorrect transformation of fabs with nnan flag
(https://bugs.llvm.org/show_bug.cgi?id=47960)

### Miscompilations with Undef Value

These are miscompilations that will disappear when undef value disappears.

1. Instsimplify: uadd_overflow(X, undef) is not undef
(https://bugs.llvm.org/show_bug.cgi?id=42209)

2. SimplifyCFG's -switch-to-lookup is incorrect w.r.t undef
(https://bugs.llvm.org/show_bug.cgi?id=42617)

3. CVP: adding nuw flags not correct in the presence of undef
(https://bugs.llvm.org/show_bug.cgi?id=42618)

4. DivRemPairs is incorrect in the presence of undef
(https://bugs.llvm.org/show_bug.cgi?id=42619)

5. Incorrect fold of uadd.with.overflow with undef
(https://bugs.llvm.org/show_bug.cgi?id=43188)

6. Incorrect fold of ashr+xor -> lshr w/ vectors
(https://bugs.llvm.org/show_bug.cgi?id=43665)

7. Incorrect 'icmp sle' -> 'icmp slt' w/ vectors
(https://bugs.llvm.org/show_bug.cgi?id=43730)

8. shuffle undef mask on vectors with poison elements
(https://bugs.llvm.org/show_bug.cgi?id=43958)

9. Can't remove shufflevector if input might be poison
(https://bugs.llvm.org/show_bug.cgi?id=44185)

10. Incorrect instcombine transform urem -> icmp+zext with vectors
(https://bugs.llvm.org/show_bug.cgi?id=44186)

11. Instcombine: incorrect transformation 'x > (x & undef)' -> 'x > undef'
(https://bugs.llvm.org/show_bug.cgi?id=44383)

12. Incorrect transformation: (undef u>> a) ^ -1 -> undef >> a, when a != 0
(https://bugs.llvm.org/show_bug.cgi?id=45447)

13. Invalid undef splat in instcombine
(https://bugs.llvm.org/show_bug.cgi?id=45455)

14. Can't remove insertelement undef
(https://bugs.llvm.org/show_bug.cgi?id=45481)

15. Incorrect instcombine fold of vector ult -> sgt
(https://bugs.llvm.org/show_bug.cgi?id=45954)

16. Incorrect transformation: mul foo, undef -> shl foo, undef
(https://bugs.llvm.org/show_bug.cgi?id=47133)

17. Incorrect transformation: (llvm.maximum undef, %x) -> undef
(https://bugs.llvm.org/show_bug.cgi?id=47567)

18. InstCombine: incorrect select operand simplification with undef
(https://bugs.llvm.org/show_bug.cgi?id=47696)

### Memory Operations

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
(https://github.com/llvm/llvm-project/commit/87407fc03c82d880cc42330a8e230e7a48174e3c
& https://github.com/llvm/llvm-project/commit/7f903873b8a937acec2e2cc232e70cba53061352)

11. [globalopt] optimization leaves store to a constant global
(https://bugs.llvm.org/show_bug.cgi?id=43616)
NOTE: Alive2 can't find this bug anymore due to changes to reduce false-positives

12. -expandmemcmp generates loads with incorrect alignment
(https://bugs.llvm.org/show_bug.cgi?id=43880)

13. InstCombine incorrectly rewrites indices of gep inbounds
(https://bugs.llvm.org/show_bug.cgi?id=44861)

14. Instcombine incorrectly transforms store i64 -> store double
(https://bugs.llvm.org/show_bug.cgi?id=45152)

15. Incorrect optimization of gep without inbounds + load -> icmp eq
(https://bugs.llvm.org/show_bug.cgi?id=45210)

16. gep(ptr, undef) isn't undef
(https://bugs.llvm.org/show_bug.cgi?id=45445)

17. X86InterleavedAccess introduces misaligned loads
(https://bugs.llvm.org/show_bug.cgi?id=45957)

18. load-store-vectorizer cannot assume that an offset calculated from add nuw is fine in general
(https://bugs.llvm.org/show_bug.cgi?id=46591)

19. LoopReroll incorrectly reorders stores across loads when they may alias
(https://bugs.llvm.org/show_bug.cgi?id=47658)

20. MemCpyOpt: uses sext instead of zext for memcpy/memset size subtraction
(https://bugs.llvm.org/show_bug.cgi?id=47697)


### Bugs found in Z3
1. https://github.com/Z3Prover/z3/issues/2369 - bug in bitblast for FPA
2. https://github.com/Z3Prover/z3/issues/2596 - bug in FPA w/ quantifiers
3. https://github.com/Z3Prover/z3/issues/2631 - bug in FPA w/ quantifiers
4. https://github.com/Z3Prover/z3/issues/2792 - Lambdas don't like quantified variables in body
5. https://github.com/Z3Prover/z3/issues/2822 - bug in MBQI
6. https://github.com/Z3Prover/z3/commit/0b14f1b6f6bb33b555bace93d644163987b0c54f - equality of arrays w/ lambdas
7. https://github.com/Z3Prover/z3/issues/2865 - crash in FPA model construction
8. https://github.com/Z3Prover/z3/issues/2878 - crash in BV theory assertion
9. https://github.com/Z3Prover/z3/issues/2879 - crash in SMT eq propagation assertion
10. https://github.com/Z3Prover/z3/commit/bb5edb7c653f9351fe674630d63cdd2b10338277 - assertion violation in qe_lite
11. https://github.com/Z3Prover/z3/issues/4192 - SMT internalize doesn't respect timeout
12. https://github.com/Z3Prover/z3/issues/2761 - crash in printing multi precision integer
13. https://github.com/Z3Prover/z3/issues/2652 - crash in partial model mode
