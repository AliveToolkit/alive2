define i8* @f(i1 %cond, i64 %a, i64 %b) {
  %p = select i1 %cond, i64 %a, i64 %b
  %call = call i8* @malloc(i64 %p)
  ret i8* %call
}

declare i8* @malloc(i64)
