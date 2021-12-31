define i4 @src(i4* %p) {
  call void @f()
  call void @g()
  call void @g()
  %v = load i4, i4* %p
  ret i4 %v
}

define i4 @tgt(i4* %p) {
  call void @g()
  call void @g()
  call void @f()
  %v = load i4, i4* %p
  ret i4 %v
}

declare void @f() inaccessiblememonly willreturn
declare void @g() willreturn
