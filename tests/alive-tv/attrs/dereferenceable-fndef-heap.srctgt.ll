define dereferenceable(4) ptr @src() {
  %p = call ptr @malloc(i64 4)
  ret ptr %p
}

define dereferenceable(4) ptr @tgt() {
  unreachable
}

; ERROR: Source is more defined than target

declare ptr @malloc(i64)
declare void @free(ptr)
