; TEST-ARGS: -dbg

define void @src() {
  %ptr = call noalias ptr @calloc(i64 4, i64 3)
  ret void
}

define void @tgt() {
  %ptr = call noalias ptr @calloc(i64 4, i64 3)
  ret void
}

declare noalias ptr @calloc(i64, i64)
; CHECK: max_alloc_size: 12
