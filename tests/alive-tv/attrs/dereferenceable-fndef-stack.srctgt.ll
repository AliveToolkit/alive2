define dereferenceable(4) i32* @src() {
  %p = alloca i32
  ret i32* %p
}

define dereferenceable(4) i32* @tgt() {
  unreachable
}
