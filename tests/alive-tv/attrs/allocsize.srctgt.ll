define ptr @src() {
  %p = call ptr @my_malloc(i32 23)
  ret ptr %p
}

define ptr @tgt() {
  %p = call dereferenceable_or_null(23) ptr @my_malloc(i32 23)
  ret ptr %p
}

declare ptr @my_malloc(i32) allocsize(0)
