define void @f(i64* %p) {
  %p2 = bitcast i64* %p to i8**
  store i8* null, i8** %p2, align 4
  ret void
}

define void @f2(i64* %p) {
  store i64 0, i64* %p
  ret void
}
