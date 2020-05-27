define void @src(i8* %p) {
  call void @f(i8* undef)
  ret void
}

define void @tgt(i8* %p) {
  unreachable
}

declare void @f(i8* nonnull)
