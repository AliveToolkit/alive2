define i2 @f(ptr %x) {
  store {i2, i2} {i2 1, i2 2}, ptr %x
  %y = load {i2, i2}, ptr %x
  %w1 = extractvalue {i2, i2} %y, 0
  %w2 = extractvalue {i2, i2} %y, 1
  %t = add i2 %w1, %w2
  ret i2 %t
}
