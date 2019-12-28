define i8 @f1() {
  %a = alloca i8
  store i8 3, i8* %a
  %b = call i8 @g(i8* %a)
  ret i8 %b
}

define i8 @f2(i8* %a) {
  %b = call i8 @g(i8* %a)
  ret i8 %b
}

define i8 @f3(i8* %a) {
  store i8 3, i8* %a
  %b = call i8 @g(i8* %a)
  ret i8 %b
}

define i8 @f4(i8* %a) {
  %b = call i8 @h(i8* %a)
  store i8 3, i8* %a
  ret i8 %b
}

define i8 @f5() {
  %p = alloca i8
  %b = call i8 @g(i8* %p)
  ret i8 %b
}

define i8 @f6(i8* %p) {
  %b = call i8 @g(i8* %p)
  ret i8 %b
}

declare i8 @g(i8*)
declare i8 @h(i8*) readnone

declare void @llvm.memcpy.p0i8.p0i8.i64(i8*, i8*, i64, i1)
