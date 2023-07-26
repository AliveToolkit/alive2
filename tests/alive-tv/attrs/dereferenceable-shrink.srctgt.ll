declare void @f(ptr)

define void @src(ptr %p) {
  call void @f(ptr dereferenceable(4) %p)
  ret void
}

define void @tgt(ptr %p) {
  call void @f(ptr dereferenceable(2) %p)
  ret void
}
