define ptr @src(ptr dereferenceable_or_null(4) %p) {
  ret ptr %p
}

define ptr @tgt(ptr dereferenceable(4) %p) {
  ret ptr %p
}

; ERROR: Source is more defined than target
