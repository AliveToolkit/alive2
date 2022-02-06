define void @src() {
  %text = alloca i8, align 1

  call void @llvm.lifetime.start.p0i8(i64 1, i8* %text)
  call void @llvm.lifetime.end.p0i8(i64 1, i8* %text)

  call void @foo(i8* %text)
  ret void
}

define void @tgt() {
  %text = alloca i8, align 1

  call void @foo(i8* %text)
  ret void
}

declare void @foo(i8*)
declare void @llvm.lifetime.start.p0i8(i64, i8*)
declare void @llvm.lifetime.end.p0i8(i64, i8*)
