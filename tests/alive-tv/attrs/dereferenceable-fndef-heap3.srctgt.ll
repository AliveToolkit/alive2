define dereferenceable(90) ptr @src() {
  %p = call ptr @malloc(i64 3)
  ret ptr %p
}

define dereferenceable(90) ptr @tgt() {
  unreachable
}

declare ptr @malloc(i64)
declare void @free(ptr)
