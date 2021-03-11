declare void @llvm.assume(i1)

declare void @f(i8* align(4)) ; has no noundef

define void @src(i8* %ptr) {
  call void @f(i8* %ptr)
  ret void
}

define void @tgt(i8* %ptr) {
  call void @f(i8* %ptr)
  call void @llvm.assume(i1 1) [ "align"(i8* %ptr, i64 4, i64 0) ]
  ret void
}

; ERROR: Source is more defined than target
