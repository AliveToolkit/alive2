define i8* @f(i8* %a, i8* nocapture %b) {
  %cmp = icmp eq i8* %a, %b
  br i1 %cmp, label %t, label %f

t:
  %v = call i8* @g(i8* %a)
  ret i8* %v

f:
  ret i8* null
}

declare i8* @g(i8*)
