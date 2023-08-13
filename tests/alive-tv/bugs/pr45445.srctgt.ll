; Found by Alive2

target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"

define <2 x ptr> @src(ptr %base, i64 %idx) {
  %basevec = insertelement <2 x ptr> undef, ptr %base, i32 0
  %idxvec = insertelement <2 x i64> undef, i64 %idx, i32 1
  %gep = getelementptr i32, <2 x ptr> %basevec, <2 x i64> %idxvec
  ret <2 x ptr> %gep
}

define <2 x ptr> @tgt(ptr %base, i64 %idx) {
  ret <2 x ptr> undef
}

; ERROR: Value mismatch
