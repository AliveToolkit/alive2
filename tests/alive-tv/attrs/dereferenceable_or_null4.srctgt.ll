define void @src(ptr dereferenceable_or_null(4) %p) {
  ret void
}

define void @tgt(ptr dereferenceable_or_null(4) %p) {
  load i32, ptr %p, align 1
  ret void
}

; ERROR: Source is more defined than target
