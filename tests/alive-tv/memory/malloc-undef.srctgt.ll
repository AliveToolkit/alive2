define void @src() {
  %ptr = call ptr @malloc(i64 undef)
  ret void
}

define void @tgt() {
  unreachable
}

declare ptr @malloc(i64)
