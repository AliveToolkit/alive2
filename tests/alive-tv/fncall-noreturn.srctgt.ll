@stderr = global i8* null, align 8
declare void @never_return()

define void @src() {
  %i = load i8*, i8** @stderr, align 8
  call void @never_return() noreturn
  unreachable
}

define void @tgt() {
  call void @never_return() noreturn
  unreachable
}
