define dereferenceable(4) i8* @src() {
  %p = call i8* @malloc(i64 4)
  ret i8* %p
}

define dereferenceable(4) i8* @tgt() {
  unreachable
}

; ERROR: Source is more defined than target

declare i8* @malloc(i64)
declare void @free(i8*)
