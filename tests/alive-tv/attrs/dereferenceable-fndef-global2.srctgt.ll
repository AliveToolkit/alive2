@g = global i16 0

define dereferenceable(4) i32* @src() {
  %p = bitcast i16* @g to i32*
  ret i32* %p
}

define dereferenceable(4) i32* @tgt() {
  unreachable
}
