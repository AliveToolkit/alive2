define void @src() noreturn {
  ret void
}

define void @tgt() noreturn {
  unreachable
}
