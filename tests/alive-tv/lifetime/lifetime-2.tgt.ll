declare void @llvm.lifetime.start.p0(ptr)
declare void @llvm.lifetime.end.p0(ptr)

define i32 @f_end() {
  %p = alloca i32

  call void @llvm.lifetime.start.p0(ptr %p)
  call void @llvm.lifetime.end.p0(ptr %p)

  store i32 10, ptr %p

  ret i32 poison
}

define i32 @f_start() {
  %p = alloca i32

  call void @llvm.lifetime.start.p0(ptr %p)
  call void @llvm.lifetime.end.p0(ptr %p)

  ret i32 poison
}

