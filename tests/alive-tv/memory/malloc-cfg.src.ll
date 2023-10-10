define ptr @f(i1 %cond, i64 %a, i64 %b) {
  br i1 %cond, label %A, label %B
A:
  br label %EXIT
B:
  br label %EXIT
EXIT:
  %p = phi i64 [%a, %A], [%b, %B]
  %call = call ptr @malloc(i64 %p)
  ret ptr %call
}

declare ptr @malloc(i64)
