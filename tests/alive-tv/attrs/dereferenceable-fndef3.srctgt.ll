define dereferenceable(4) i32* @src(i32* %p) {
  store i32 0, i32* %p, align 1
  ret i32* %p
}

define dereferenceable(4) i32* @tgt(i32* %p) {
  unreachable
}

; ERROR: Source is more defined than target
