declare void @llvm.assume(i1)

declare void @f(i8* nonnull noundef)

define void @src(i8* %ptr) {
  call void @f(i8* %ptr)
  ret void
}

define void @tgt(i8* %ptr) {
  call void @f(i8* %ptr)
  call void @llvm.assume(i1 1) [ "nonnull"(i8* %ptr) ]
  ret void
}
