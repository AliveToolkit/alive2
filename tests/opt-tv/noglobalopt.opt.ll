; TEST-ARGS: -passes=attributor -attributor-manifest-internal

define void @f() {
  call void @g()
  ret void
}

define void @g() {
  ret void
}

; CHECK-NOT: Transformation doesn't verify!
