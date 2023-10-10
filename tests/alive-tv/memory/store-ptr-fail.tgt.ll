define void @f(ptr %ptr) {
  %dst2 = bitcast ptr %ptr to ptr
  store ptr %ptr, ptr %ptr, align 1

  %ptr1 = getelementptr i8, ptr %ptr, i64 1
  %dst = bitcast ptr %ptr1 to ptr
  store ptr %ptr, ptr %dst, align 1
  ret void
}
