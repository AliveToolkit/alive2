; TEST-ARGS: --disable-undef-input --disable-poison-input


define i32 @test16_i1(i1 %0) {
  %2 = zext i1 %0 to i32
  br label %3

3:                                                ; preds = %3, %1
  br i1 %0, label %4, label %3

4:                                                ; preds = %3
  ret i32 %2
}
