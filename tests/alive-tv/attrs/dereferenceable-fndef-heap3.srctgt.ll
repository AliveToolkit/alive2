define dereferenceable(4) ptr @src() {
  %p = call ptr @malloc(i64 3)
  ret ptr %p
}

define dereferenceable(4) ptr @tgt() {
  unreachable
}

declare ptr @malloc(i64)
declare void @free(ptr)
