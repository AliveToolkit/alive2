
declare void @fn(ptr)

define void @src(ptr align 16 dereferenceable(4) %0) {
  call void @fn(ptr %0)
  ret void
}

define void @tgt(ptr align 16 dereferenceable(4) %0) {
  call void @fn(ptr nonnull %0)
  ret void
}
