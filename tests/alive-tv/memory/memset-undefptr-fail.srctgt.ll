; This test shows that our approximation does not support conversion from
; memset(undef) to store undef, because memset undef is approximated to store
; all identical undef bits.

define void @src(i32** %ptr) {
  %p = bitcast i32** %ptr to i8*
  call void @llvm.memset.p0i8.i32(i8* %p, i8 undef, i32 8, i1 0)
  ret void
}

define void @tgt(i32** %ptr) {
  store i32* undef, i32** %ptr, align 1
  ret void
}

; ERROR: Mismatch in memory
declare void @llvm.memset.p0i8.i32(i8*, i8, i32, i1)
