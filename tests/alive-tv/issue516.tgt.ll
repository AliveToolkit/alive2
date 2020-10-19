declare void @f()

define void @src() {
  call void @f()
  ret void
}
