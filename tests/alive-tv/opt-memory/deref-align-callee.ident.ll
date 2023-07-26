; TEST-ARGS: -dbg

declare void @f(ptr)

define void @g(ptr dereferenceable(4) align 4 %p) {
  call void @f(ptr dereferenceable(4) %p)
  ret void
}

; CHECK: min_access_size: 1
