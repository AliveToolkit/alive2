define i8 @src(ptr %p, i8 %a) {
  %v = call i8 %p(i8 %a)
  ret i8 %v
}

define i8 @tgt(ptr %p, i8 %a) {
  %cmp = icmp eq ptr %p, @fn
  br i1 %cmp, label %then, label %else

then:
  %v = call i8 @fn(i8 %a)
  ret i8 %v

else:
  %v2 = call i8 %p(i8 %a)
  ret i8 %v2
}

declare i8 @fn(i8 returned)
