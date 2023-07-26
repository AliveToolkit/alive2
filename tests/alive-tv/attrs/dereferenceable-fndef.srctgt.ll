define dereferenceable(4) ptr @src(ptr dereferenceable(4) %p) {
  ret ptr %p
}

define dereferenceable(4) ptr @tgt(ptr dereferenceable(4) %p) {
  unreachable
}

; ERROR: Source is more defined than target
