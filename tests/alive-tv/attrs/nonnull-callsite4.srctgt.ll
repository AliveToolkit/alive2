define void @src(i8* %p) {
  call void @f(i8* null)
  ret void
}

define void @tgt(i8* %p) {
  unreachable
}

declare void @f(i8* nonnull)
