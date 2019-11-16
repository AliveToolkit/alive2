@g = constant i16 1

define i8 @f() {
  %x  = load i8, i8* getelementptr inbounds (i8, i8* bitcast (i16* @g to i8*), i64 1)
  ; After conversion, it becomes
  ;   %1 = bitcast * @g to *
  ;   %2 = gep inbounds * %1, 1 x i64 1
  ;   %x = load i8, * %2, align 1
  ret i8 %x
}

; ERROR: Value mismatch
