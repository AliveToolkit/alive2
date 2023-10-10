define i16 @f1(half %x) {
  %a = alloca half
  store half %x, ptr %a
  %i = load i16, ptr %a
  ret i16 %i
}
