define void @src(ptr %p) {
  call void @f(ptr null)
  ret void
}

define void @tgt(ptr %p) {
  call void @f(ptr poison)
  ret void
}

declare void @f(ptr nonnull)
