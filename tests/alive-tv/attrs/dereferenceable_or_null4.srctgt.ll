define void @src(i32* dereferenceable_or_null(4) %p) {
  ret void
}

define void @tgt(i32* dereferenceable_or_null(4) %p) {
  load i32, i32* %p, align 1
  ret void
}

; ERROR: Source is more defined than target
