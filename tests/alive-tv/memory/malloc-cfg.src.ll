define i8* @f(i1 %cond, i64 %a, i64 %b) {
  br i1 %cond, label %A, label %B
A:
  br label %EXIT
B:
  br label %EXIT
EXIT:
  %p = phi i64 [%a, %A], [%b, %B]
  %call = call i8* @malloc(i64 %p)
  ret i8* %call
}

declare i8* @malloc(i64)
