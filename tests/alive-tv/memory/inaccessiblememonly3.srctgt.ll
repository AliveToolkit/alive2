define i4 @src(ptr %p) {
  call void @f()
  call void @g()
  call void @g()
  %v = load i4, ptr %p
  ret i4 %v
}

define i4 @tgt(ptr %p) {
  call void @g()
  call void @g()
  call void @f()
  %v = load i4, ptr %p
  ret i4 %v
}

declare void @f() memory(inaccessiblemem: readwrite) willreturn
declare void @g() willreturn
