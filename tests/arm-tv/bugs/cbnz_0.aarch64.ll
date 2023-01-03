; TEST-ARGS: --disable-undef-input --disable-poison-input


define i32 @main() {
  br label %1

1:                                                ; preds = %1, %0
  %2 = icmp eq i32 0, 4
  br i1 %2, label %1, label %3

3:                                                ; preds = %1
  ret i32 0
}
