@x = constant i8 0

define void @src() {
  call void @f(i8* @x)
  ret void
}

define void @tgt() readnone {
  call void @f(i8* @x)
  ret void
}

declare void @f(i8*) argmemonly
