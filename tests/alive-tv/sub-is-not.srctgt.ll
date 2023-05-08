define i8 @src(i8 %x) {
  %m = sub i8 %x, -1
  ret i8 %m
}

define i8 @tgt(i8 %x) {
  %m = xor i8 %x, -1
  ret i8 %m
}

; Transformation doesn't verify!

define i8 @src3(i8 %x) {
  %m = sub i8 0, %x
  ret i8 %m
}

define i8 @tgt3(i8 %x) {
  %m = xor i8 %x, -1
  ret i8 %m
}

; Transformation doesn't verify!

define i8 @src9(i8 %x) {
  %m = sub i8 -1, %x
  ret i8 %m
}

define i8 @tgt9(i8 %x) {
  %m = xor i8 %x, -1
  ret i8 %m
}

; Transformation seems to be correct!

define i8 @src4(i8 %x, i8 %y) {
  %m = add i8 %y, %x
  ret i8 %m
}

define i8 @tgt4(i8 %x) {
  %m = xor i8 %x, -1
  ret i8 %m
}

; ERROR: Signature mismatch between src and tgt
