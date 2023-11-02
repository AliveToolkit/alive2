define i2 @f(ptr %x) {
  store {i2, i2} {i2 1, i2 2}, ptr %x
  ret i2 3
}
