define void @src(ptr %p) {
  %q = getelementptr inbounds i8, ptr %p, i64 1
  call void @f(ptr %q)
  ret void
}

define void @tgt(ptr %p) {
  %q = getelementptr inbounds i8, ptr %p, i64 1
  call void @f(ptr nonnull %q)
  ret void
}

declare void @f(ptr)
