define dereferenceable(4) i32* @src() {
  %p = call i32* @f()
  ret i32* %p
}

define dereferenceable(4) i32* @tgt() {
  unreachable
}

; ERROR: Source is more defined than target

declare dereferenceable(4) i32* @f()
