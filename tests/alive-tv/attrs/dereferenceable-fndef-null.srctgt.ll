define dereferenceable(4) ptr @src(ptr %p) {
  ret ptr null
}

define dereferenceable(4) ptr @tgt(ptr %p) {
  unreachable
}
