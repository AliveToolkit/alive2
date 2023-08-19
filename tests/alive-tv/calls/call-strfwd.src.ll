; ERROR: Source is more defined

define i8 @f(ptr %p) {
  %a = call ptr @malloc(i64 1)
  call void @llvm.memcpy.p0i8.p0i8.i64(ptr %a, ptr %p, i64 1, i1 false)
  %b = call i8 @g(ptr %a)
  ret i8 %b
}

declare i8 @g(ptr)
declare ptr @malloc(i64)
declare void @llvm.memcpy.p0i8.p0i8.i64(ptr, ptr, i64, i1)
