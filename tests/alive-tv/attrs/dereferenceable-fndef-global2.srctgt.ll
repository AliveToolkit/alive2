@g = global i16 0

define dereferenceable(4) ptr @src() {
  ret ptr @g
}

define dereferenceable(4) ptr @tgt() {
  unreachable
}
