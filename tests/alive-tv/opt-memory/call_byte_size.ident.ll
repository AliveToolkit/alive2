; TEST-ARGS: -dbg
; CHECK: bits_byte: 64

define void @f(ptr %p) {
  store ptr null, ptr %p, align 8
  call void @g()
  ret void
}

declare void @g()
