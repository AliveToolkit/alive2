@g = global i32 0

define dereferenceable(4) ptr @src() {
  ret ptr @g
}

define dereferenceable(4) ptr @tgt() {
  unreachable
}

; ERROR: Source is more defined than target
