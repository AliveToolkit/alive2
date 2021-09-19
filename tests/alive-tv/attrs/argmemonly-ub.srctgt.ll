@glb = external global i8

define void @src(i8 *%p) argmemonly {
  call void @f(i8* %p)
  ret void
}

define void @tgt(i8 *%p) argmemonly {
  unreachable
}

declare void @f(i8*)
