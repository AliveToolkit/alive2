define ptr @f(i1 %cond, i64 %a, i64 %b) {
  %p = select i1 %cond, i64 %a, i64 %b
  %call = call ptr @malloc(i64 %p)
  ret ptr %call
}

declare ptr @malloc(i64)
