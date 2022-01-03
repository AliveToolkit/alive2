define void @fn(i8* %p, i8* %q) {
  call void @f(i8* %p)
  call void @f(i8* %q) argmemonly
  ret void
}

declare void @f(i8*)
