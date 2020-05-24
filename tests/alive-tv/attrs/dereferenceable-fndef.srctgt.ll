define dereferenceable(4) i32* @src(i32* dereferenceable(4) %p) {
  ret i32* %p
}

define dereferenceable(4) i32* @tgt(i32* dereferenceable(4) %p) {
  unreachable
}

; ERROR: Source is more defined than target
