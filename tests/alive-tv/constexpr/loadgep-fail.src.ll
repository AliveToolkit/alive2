@g = constant i16 1

define i8 @f() {
  %x  = load i8, ptr getelementptr inbounds (i8, ptr @g, i64 1)
  ret i8 %x
}

; ERROR: Value mismatch
