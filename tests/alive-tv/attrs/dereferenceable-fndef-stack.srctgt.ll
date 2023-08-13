define dereferenceable(4) ptr @src() {
  %p = alloca i32
  ret ptr %p
}

define dereferenceable(4) ptr @tgt() {
  unreachable
}
