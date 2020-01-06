; ERROR: Mismatch in memory

define void @f(i8* %ptr) {
  %ptr1 = getelementptr i8, i8* %ptr, i64 1
  %dst = bitcast i8* %ptr1 to i8**
  store i8* %ptr, i8** %dst, align 1

  %dst2 = bitcast i8* %ptr to i8**
  store i8* %ptr, i8** %dst2, align 1
  ret void
}
