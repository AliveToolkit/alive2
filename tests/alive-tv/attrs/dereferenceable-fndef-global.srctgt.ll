@g = global i32 0

define dereferenceable(4) i32* @src() {
  ret i32* @g
}

define dereferenceable(4) i32* @tgt() {
  unreachable
}

; ERROR: Source is more defined than target
