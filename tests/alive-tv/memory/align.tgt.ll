define i32 @foo1(i32* %a) {
  %v = load i32, i32* %a, align 32
  %ptrint = ptrtoint i32* %a to i64
  %maskedptr = and i64 %ptrint, 31
  %maskcond = icmp eq i64 %maskedptr, 0
  tail call void @llvm.assume(i1 %maskcond)
  ret i32 %v
}

declare void @llvm.assume(i1)
