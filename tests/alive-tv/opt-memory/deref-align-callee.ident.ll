; TEST-ARGS: -dbg

declare void @f(i32*)

define void @g(i32* dereferenceable(4) align 4 %p) {
  call void @f(i32* dereferenceable(4) %p)
  ret void
}

; CHECK: min_access_size: 1
