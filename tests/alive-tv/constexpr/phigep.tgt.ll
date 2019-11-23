@g = constant i16 0

define i8 @f(i1 %cond) {
  br i1 %cond, label %A, label %B
A:
  %g1 = bitcast i16* @g to i8*
  %a1 = getelementptr inbounds i8, i8* %g1, i64 0
  br label %EXIT
B:
  %g2 = bitcast i16* @g to i8*
  %a2 = getelementptr inbounds i8, i8* %g2, i64 1
  br label %EXIT
EXIT:
  %addr = phi i8* [%a1, %A], [%a2, %B]
  %x  = load i8, i8* %addr
  ret i8 %x
}
