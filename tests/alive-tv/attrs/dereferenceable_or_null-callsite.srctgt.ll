define void @src(ptr %p) {
  call void @f(ptr dereferenceable_or_null(4) %p)
  ret void
}

define void @tgt(ptr %p) {
  call void @f(ptr dereferenceable(4) %p)
  ret void
}

declare void @f(ptr)

; ERROR: Source is more defined than target
