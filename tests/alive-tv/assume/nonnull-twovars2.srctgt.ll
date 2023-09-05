declare void @llvm.assume(i1)

declare void @f(ptr nonnull) ; has no noundef

define void @src(ptr %ptr, ptr %ptr2) {
  call void @f(ptr %ptr)
  call void @f(ptr noundef %ptr2)
  ret void
}

define void @tgt(ptr %ptr, ptr %ptr2) {
  call void @f(ptr %ptr)
  call void @f(ptr noundef %ptr2)
  call void @llvm.assume(i1 1) [ "nonnull"(ptr %ptr), "nonnull"(ptr %ptr2) ]
  ret void
}

; ERROR: Source is more defined than target
