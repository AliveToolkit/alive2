@g = constant i16 1

define i8 @f(i1 %cond) {
  br i1 %cond, label %A, label %B
A:
  br label %EXIT
B:
  br label %EXIT
EXIT:
  %addr = phi i8* [getelementptr inbounds (i8, i8* bitcast (i16* @g to i8*), i64 0), %A],
                  [getelementptr inbounds (i8, i8* bitcast (i16* @g to i8*), i64 1), %B]
  %x  = load i8, i8* %addr
  ret i8 %x
}

; ERROR: Value mismatch
