; ERROR: Source is more defined

define i8 @f(i8* %p) {
  %a = call i8* @malloc(i64 1)
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* %a, i8* %p, i64 1, i1 false)
  %b = call i8 @g(i8* %a)
  ret i8 %b
}

declare i8 @g(i8*)
declare i8* @malloc(i64)
declare void @llvm.memcpy.p0i8.p0i8.i64(i8*, i8*, i64, i1)
