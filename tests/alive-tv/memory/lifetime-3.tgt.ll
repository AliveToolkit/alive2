declare void @llvm.lifetime.start.p0i8(i64, i8*)
declare void @llvm.lifetime.end.p0i8(i64, i8*)

define i32 @f() {
  %p = alloca i32

  %p0 = bitcast i32* %p to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* %p0)
  store i32 10, i32* %p
  call void @llvm.lifetime.end.p0i8(i64 4, i8* %p0)

  ; store after lifetime.end can be optimized out
  %v = load i32, i32* %p

  ret i32 %v
}

