define i2 @f({i2, i2}* %x) {
  store {i2, i2} {i2 1, i2 2}, {i2, i2}* %x
  ret i2 3
}

