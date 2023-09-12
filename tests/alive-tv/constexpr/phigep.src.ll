@g = constant i16 0

define i8 @f(i1 %cond) {
  br i1 %cond, label %A, label %B
A:
  br label %EXIT
B:
  br label %EXIT
EXIT:
  %addr = phi ptr [getelementptr inbounds (i8, ptr @g, i64 0), %A],
                  [getelementptr inbounds (i8, ptr @g, i64 1), %B]
  %x  = load i8, ptr %addr
  ret i8 %x
}
