; TEST-ARGS: -backend-tv --disable-undef-input --disable-poison-input

define i64 @brR0_i64_slt(i64 %0) {
  %2 = icmp slt i64 %0, 0
  br i1 %2, label %3, label %4

3:                                                ; preds = %1
  ret i64 1

4:                                                ; preds = %1
  %5 = udiv i64 763841731, %0
  ret i64 %5
}
