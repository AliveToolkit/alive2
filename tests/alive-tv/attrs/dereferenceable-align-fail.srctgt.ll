define void @src(i32* %p) {
  load i32, i32* %p, align 1
  call void @f(i32* dereferenceable(4) %p)
  ret void
}

define void @tgt(i32* %p) {
  load i32, i32* %p, align 4
  call void @f(i32* dereferenceable(4) %p)
  ret void
}

declare void @f(i32* %ptr)

; ERROR: Source is more defined than target
