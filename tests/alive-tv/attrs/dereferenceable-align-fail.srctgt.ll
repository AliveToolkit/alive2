define void @src(ptr %p) {
  load i32, ptr %p, align 1
  call void @f(ptr dereferenceable(4) %p)
  ret void
}

define void @tgt(ptr %p) {
  load i32, ptr %p, align 4
  call void @f(ptr dereferenceable(4) %p)
  ret void
}

declare void @f(ptr %ptr)

; ERROR: Source is more defined than target
