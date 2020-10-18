declare i8 @func(i8) readonly

define i8 @src() {
  %V1 = call i8 @func(i8 undef)
  %V2 = call i8 @func(i8 undef)
  %diff = sub i8 %V1, %V2
  ret i8 %diff
}

define i8 @tgt() {
  %V1 = call i8 @func(i8 undef)
  ret i8 0
}
