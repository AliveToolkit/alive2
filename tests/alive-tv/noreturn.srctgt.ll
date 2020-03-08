declare void @f() noreturn

define void @src() {
  call void @f()
  call void @f()
  ret void
}

define void @tgt() {
  call void @f()
  unreachable
}
