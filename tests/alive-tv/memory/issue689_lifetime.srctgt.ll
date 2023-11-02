define void @src() {
entry:
  %retval = alloca i32, align 4
  %b = alloca i32, align 4
  call void @llvm.lifetime.start.p0i8(i64 4, ptr %b)
  %0 = ptrtoint ptr %b to i32
  %call = call i32 @a(i32 %0)
  ret void
}

define void @tgt() {
entry:
  %b = alloca i32, align 4
  call void @llvm.lifetime.start.p0i8(i64 4, ptr %b)
  %0 = ptrtoint ptr %b to i32
  %call = call i32 @a(i32 %0)
  ret void
}

declare i32 @a(i32)
declare void @llvm.lifetime.start.p0i8(i64, ptr)

