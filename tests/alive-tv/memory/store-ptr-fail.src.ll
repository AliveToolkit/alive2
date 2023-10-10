; ERROR: Mismatch in memory

define void @f(ptr %ptr) {
  %ptr1 = getelementptr i8, ptr %ptr, i64 1
  %dst = bitcast ptr %ptr1 to ptr
  store ptr %ptr, ptr %dst, align 1

  %dst2 = bitcast ptr %ptr to ptr
  store ptr %ptr, ptr %dst2, align 1
  ret void
}
