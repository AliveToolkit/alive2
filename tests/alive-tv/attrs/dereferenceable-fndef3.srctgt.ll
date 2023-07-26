define dereferenceable(4) ptr @src(ptr %p) {
  store i32 0, ptr %p, align 1
  ret ptr %p
}

define dereferenceable(4) ptr @tgt(ptr %p) {
  unreachable
}

; ERROR: Source is more defined than target
