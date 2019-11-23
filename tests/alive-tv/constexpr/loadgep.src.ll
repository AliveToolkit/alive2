@g = constant i16 0

define i8 @f() {
  %x  = load i8, i8* getelementptr inbounds (i8, i8* bitcast (i16* @g to i8*), i64 1)
  ret i8 %x
}
