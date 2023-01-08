declare i64  @llvm.fshl.i64 (i64 %a, i64 %b, i64 %c)
declare i64  @llvm.fshr.i64 (i64 %a, i64 %b, i64 %c)

define i64 @foo1(i64 %a, i64 %b, i64 %c) local_unnamed_addr #0 {
  %r = call i64 @llvm.fshl.i64(i64 %a, i64 %b, i64 %c) 
  ret i64 %r
}

define i64 @foo2(i64 %a, i64 %b, i64 %c) local_unnamed_addr #0 {
  %r = call i64 @llvm.fshr.i64(i64 %a, i64 %b, i64 %c) 
  ret i64 %r
}
