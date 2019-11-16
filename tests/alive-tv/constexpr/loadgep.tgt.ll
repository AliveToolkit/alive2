@g = constant i16 0

define i8 @f() {
  %1 = bitcast i16* @g to i8*
  %2 = getelementptr inbounds i8, i8* %1, i64 1
  %x = load i8, i8* %2, align 1
  ret i8 0
}
