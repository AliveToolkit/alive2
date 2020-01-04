define i16 @f1(half %x) {
  %i = bitcast half %x to i16
  ret i16 %i
}
