define void @src(i32* %p) {
  call void @f(i32* dereferenceable_or_null(4) %p)
  ret void
}

define void @tgt(i32* %p) {
  call void @f(i32* dereferenceable(4) %p)
  ret void
}

declare void @f(i32*)

; ERROR: Source is more defined than target
