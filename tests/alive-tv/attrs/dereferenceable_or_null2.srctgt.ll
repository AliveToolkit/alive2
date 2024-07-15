define ptr @src(ptr dereferenceable(4) %p) {
  ret ptr %p
}

define ptr @tgt(ptr dereferenceable_or_null(4) %p) {
  ret ptr %p
}
