; TEST-ARGS: -dbg

declare dereferenceable(8) align 4 ptr @f()

define void @g() {
  %r = call ptr @f()
  store i32 0, ptr %r, align 4
  ret void
}

; CHECK: min_access_size: 4
