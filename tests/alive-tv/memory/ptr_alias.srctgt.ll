@a = global i32 0, align 4

declare void @llvm.assume(i1)

define void @src(i1 %cmp) {
  call void @llvm.assume(i1 %cmp)
  store i32 0, ptr @a, align 4
  ret void
}

define void @tgt(i1 %cmp) {
  br i1 %cmp, label %exit, label %unreachable

unreachable:
  store i64 0, ptr @a, align 4
  br label %exit

exit:
  store i32 0, ptr @a, align 4
  ret void
}
