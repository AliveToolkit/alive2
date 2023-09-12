declare void @llvm.assume(i1)

declare void @f(ptr noundef nonnull)

define void @src(ptr %ptr, ptr %ptr2) {
  call void @f(ptr %ptr)
  call void @f(ptr %ptr2)
  ret void
}

define void @tgt(ptr %ptr, ptr %ptr2) {
  call void @f(ptr %ptr)
  call void @f(ptr %ptr2)
  call void @llvm.assume(i1 1) [ "nonnull"(ptr %ptr), "nonnull"(ptr %ptr2) ]
  ret void
}
