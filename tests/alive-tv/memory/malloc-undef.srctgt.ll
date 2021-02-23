define void @src() {
  %ptr = call i8* @malloc(i64 undef)
  ret void
}

define void @tgt() {
  unreachable
}

declare i8* @malloc(i64)
