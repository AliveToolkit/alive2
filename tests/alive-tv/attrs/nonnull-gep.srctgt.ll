define void @src(i8* %p) {
  %q = getelementptr inbounds i8, i8* %p, i64 1
  call void @f(i8* %q)
  ret void
}

define void @tgt(i8* %p) {
  %q = getelementptr inbounds i8, i8* %p, i64 1
  call void @f(i8* nonnull %q)
  ret void
}

declare void @f(i8*)
