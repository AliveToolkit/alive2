@x = constant i8 0
@y = constant i8 1

define void @src() {
  call void @f(i8* @x)
  call void @f(i8* @y)
  ret void
}

define void @tgt() {
  call void @f(i8* @y)
  call void @f(i8* @x)
  ret void
}

declare void @f(i8*) argmemonly willreturn
