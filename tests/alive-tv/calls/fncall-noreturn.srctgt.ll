@stderr = global ptr null, align 8
declare void @never_return()

define void @src() {
  %i = load ptr, ptr @stderr, align 8
  call void @never_return() noreturn
  unreachable
}

define void @tgt() {
  call void @never_return() noreturn
  unreachable
}
