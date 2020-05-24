define dereferenceable(4) i8* @src() {
  %p = call i8* @malloc(i64 3)
  ret i8* %p
}

define dereferenceable(4) i8* @tgt() {
  unreachable
}

declare i8* @malloc(i64)
declare void @free(i8*)
