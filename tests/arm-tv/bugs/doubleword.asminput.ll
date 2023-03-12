define void @store-pre-indexed-doubleword2(ptr %0) {
  %2 = load ptr, ptr %0, align 8
  store i64 0, ptr %2, align 4
  ret void
}
