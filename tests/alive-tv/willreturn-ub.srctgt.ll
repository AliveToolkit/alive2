define void @src() willreturn {
  call void @f() noreturn
  ret void
}

define void @tgt() willreturn {
  unreachable
}

declare void @f()
