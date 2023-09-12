declare void @llvm.assume(i1)

declare void @f(ptr align(4) dereferenceable(1)) ; dereferenceable implies noundef

define void @src(ptr %ptr) {
  call void @f(ptr %ptr)
  ret void
}

define void @tgt(ptr %ptr) {
  call void @f(ptr %ptr)
  call void @llvm.assume(i1 1) [ "align"(ptr %ptr, i64 4) ]
  ret void
}
