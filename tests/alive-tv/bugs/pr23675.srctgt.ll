; https://bugs.llvm.org/show_bug.cgi?id=23675

define i4 @src(i2, i1, i4, i2, i1, i4, i2, i1, i4, i2, i1, i4) {
  %13 = mul nsw i4 -2, %2
  %14 = add i4 %13, 3
  ret i4 %14
}

define i4 @tgt(i2, i1, i4, i2, i1, i4, i2, i1, i4, i2, i1, i4) {
  %13 = mul nsw i4 %2, 2
  %14 = sub i4 3, %13
  ret i4 %14
}

; ERROR: Target is more poisonous than source
