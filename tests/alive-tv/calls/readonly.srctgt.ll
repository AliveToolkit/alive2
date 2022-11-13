declare i8 @func(i8) memory(read)

define i8 @src(i8 %n) {
  %V1 = call i8 @func(i8 %n)
  %V2 = call i8 @func(i8 %n)
  %diff = sub i8 %V1, %V2
  ret i8 %diff
}

define i8 @tgt(i8 %n) {
  %V1 = call i8 @func(i8 %n)
  ret i8 0
}
