define i32 @src(i32 %0) {
entry:
  %1 = call noundef i32 @llvm.ctlz.i32(i32 %0, i1 true), !range !0
  ret i32 %1
}

define i32 @tgt(i32 %0) {
entry:
  %1 = call noundef i32 @llvm.ctlz.i32(i32 %0, i1 true), !range !0
  ret i32 %1
}

!0 = !{i32 0, i32 33}
