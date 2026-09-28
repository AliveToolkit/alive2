; TEST-ARGS: --single-vscale=2
; CHECK: Transformation seems to be correct!
; CHECK-NOT: ERROR:

declare void @consume(ptr)

define void @src(ptr %p) {
  call void @consume(ptr byval(<vscale x 4 x i32>) %p)
  ret void
}

define void @tgt(ptr %p) {
  call void @consume(ptr byval([32 x i8]) align 16 %p)
  ret void
}
