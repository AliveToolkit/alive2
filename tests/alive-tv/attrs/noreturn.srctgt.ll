declare void @f() noreturn

define void @src() {
  call void @f()
  call void @f()
  ret void
}

define void @tgt() {
  call void @f()
  udiv i32 1, 0 ; cannot use unreachable due to a bug; it does not call addReturn
  ret void
}
