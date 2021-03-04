declare void @llvm.assume(i1)

declare void @f(i8* nonnull) ; has no noundef

define void @src(i8* %ptr, i8* %ptr2) {
  call void @f(i8* %ptr)
  call void @f(i8* noundef %ptr2)
  ret void
}

define void @tgt(i8* %ptr, i8* %ptr2) {
  call void @f(i8* %ptr)
  call void @f(i8* noundef %ptr2)
  call void @llvm.assume(i1 1) [ "nonnull"(i8* %ptr), "nonnull"(i8* %ptr2) ]
  ret void
}

; ERROR: Source is more defined than target
