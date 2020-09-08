declare void @f(i8*)

define void @src(i8* %p) {
  call void @f(i8* dereferenceable(4) %p)
  ret void
}

define void @tgt(i8* %p) {
  call void @f(i8* dereferenceable(4) %p)
  ret void
}
