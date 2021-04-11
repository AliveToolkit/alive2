define void @src() {
entry:
  %retval = alloca i32, align 4
  %b = alloca i32, align 4
  %0 = bitcast i32* %b to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* %0)
  %1 = ptrtoint i32* %b to i32
  %call = call i32 @a(i32 %1)
  ret void
}

define void @tgt() {
entry:
  %b = alloca i32, align 4
  %0 = bitcast i32* %b to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* %0)
  %1 = ptrtoint i32* %b to i32
  %call = call i32 @a(i32 %1)
  ret void
}

declare i32 @a(i32)
declare void @llvm.lifetime.start.p0i8(i64, i8*)

