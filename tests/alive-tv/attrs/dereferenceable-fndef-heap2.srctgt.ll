define dereferenceable(4) ptr @src() {
  %p = call ptr @malloc(i64 4)
  call void @free(ptr %p)
  ret ptr %p
}

define dereferenceable(4) ptr @tgt() {
  unreachable
}

declare ptr @malloc(i64)
declare void @free(ptr)
