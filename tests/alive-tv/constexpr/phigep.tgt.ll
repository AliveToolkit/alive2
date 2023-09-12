@g = constant i16 0

define i8 @f(i1 %cond) {
  br i1 %cond, label %A, label %B
A:
  %a1 = getelementptr inbounds i8, ptr @g, i64 0
  br label %EXIT
B:
  %a2 = getelementptr inbounds i8, ptr @g, i64 1
  br label %EXIT
EXIT:
  %addr = phi ptr [%a1, %A], [%a2, %B]
  %x  = load i8, ptr %addr
  ret i8 %x
}
