; ERROR: Value mismatch

define i16 @src(ptr align(4) %p) {
  %a = load i16, ptr %p, align 1
  %p2 = getelementptr i16, ptr %p, i32 1
  %b = load i16, ptr %p2, align 1
  %x = xor i16 %a, %b
  ret i16 %x
}

define i16 @tgt(ptr align(4) %p) {
  ret i16 0
}
