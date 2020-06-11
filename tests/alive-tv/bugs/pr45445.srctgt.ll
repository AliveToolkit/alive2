; TEST-ARGS: -smt-to=2000
; Found by Alive2

target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"

define <2 x i32*> @src(i32* %base, i64 %idx) {
  %basevec = insertelement <2 x i32*> undef, i32* %base, i32 0
  %idxvec = insertelement <2 x i64> undef, i64 %idx, i32 1
  %gep = getelementptr i32, <2 x i32*> %basevec, <2 x i64> %idxvec
  ret <2 x i32*> %gep
}

define <2 x i32*> @tgt(i32* %base, i64 %idx) {
  ret <2 x i32*> undef
}

; ERROR: Value mismatch
