declare void @llvm.assume(i1)

declare void @f(ptr align(4) noundef)

define void @src(ptr %ptr) {
  call void @f(ptr %ptr)
  ret void
}

define void @tgt(ptr %ptr) {
  call void @f(ptr %ptr)
  %ptr2 = getelementptr i8, ptr %ptr, i64 1
  call void @llvm.assume(i1 1) [ "align"(ptr %ptr2, i64 4, i64 2) ] ; UB
  ret void
}

; ERROR: Source is more defined than target
