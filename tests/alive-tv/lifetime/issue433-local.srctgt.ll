define i8 @src() {
entry:
    %p = alloca i8
    call void @llvm.lifetime.start.p0i8(i64 1, ptr %p)
    store i8 1, ptr %p
    call void @llvm.lifetime.start.p0i8(i64 1, ptr %p)
    %v = load i8, ptr %p
    call void @llvm.lifetime.end.p0i8(i64 1, ptr %p)
    ret i8 %v
}

define i8 @tgt() {
  %p = alloca i8, align 1
  call void @llvm.lifetime.start.p0i8(i64 1, ptr %p)
  store i8 1, ptr %p, align 1
  call void @llvm.lifetime.start.p0i8(i64 1, ptr %p)
  call void @llvm.lifetime.end.p0i8(i64 1, ptr %p)
  ret i8 undef
}

declare void @llvm.lifetime.start.p0i8(i64, ptr)
declare void @llvm.lifetime.end.p0i8(i64, ptr)
