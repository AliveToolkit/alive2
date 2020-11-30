; TEST-ARGS: -dbg

declare void @f(i32*)

define void @src(i32* dereferenceable(4) align 4 %p) {
  call void @f(i32* dereferenceable(4) %p)
  ret void
}

define void @tgt(i32* dereferenceable(4) align 4 %p) {
  call void @f(i32* dereferenceable(4) %p)
  ret void
}


; CHECK: min_access_size: 1
