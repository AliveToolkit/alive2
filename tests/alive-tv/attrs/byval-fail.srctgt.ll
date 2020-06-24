define i8 @src(i8* byval %p) {
  %a = alloca i8
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* %a, i8* %p, i64 1, i1 false)
  %b = call i8 @g(i8* %a)
  ret i8 %b
}

define i8 @tgt(i8* byval %p) {
  %b = call i8 @g(i8* %p)
  ret i8 %b
}

declare void @llvm.memcpy.p0i8.p0i8.i64(i8*, i8*, i64, i1)
declare i8 @g(i8*)

; ERROR: Source is more defined than target
