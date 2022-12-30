; TEST-ARGS: -backend-tv --disable-undef-input


define i64 @umull(i64 %0, i64 %1) {
  %3 = and i64 %0, 4294967295
  %4 = and i64 %1, 4294967295
  %5 = mul nuw i64 %4, %3
  ret i64 %5
}
