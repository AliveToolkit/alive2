; TEST-ARGS: -dbg

declare dereferenceable(8) align 4 i32* @f()

define void @g() {
  %r = call i32* @f()
  store i32 0, i32* %r, align 4
  ret void
}

; CHECK: min_access_size: 4
