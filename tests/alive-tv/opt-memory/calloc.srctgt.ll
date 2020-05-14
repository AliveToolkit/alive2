; TEST-ARGS: -dbg

define void @src() {
  %ptr = call noalias i8* @calloc(i64 4, i64 3)
  ret void
}

define void @tgt() {
  %ptr = call noalias i8* @calloc(i64 4, i64 3)
  ret void
}

declare noalias i8* @calloc(i64, i64)
; CHECK: max_alloc_size: 12
