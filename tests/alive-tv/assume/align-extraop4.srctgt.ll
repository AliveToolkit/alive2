; SKIP-IDENTITY
declare void @llvm.assume(i1)

declare void @f(i8* align(4) noundef)

define void @src(i8* %ptr) {
  call void @f(i8* %ptr)
  ret void
}

define void @tgt(i8* %ptr) {
  call void @f(i8* %ptr)
  %ptr2 = getelementptr i8, i8* %ptr, i64 1
  call void @llvm.assume(i1 1) [ "align"(i8* %ptr2, i128 4, i128 2) ] ; UB
  ret void
}

; Check that this does not crash
; ERROR: Unsupported instruction
