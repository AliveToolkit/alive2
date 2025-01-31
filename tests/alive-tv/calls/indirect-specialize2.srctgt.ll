define i8 @src(ptr %p) {
  %v = call i8 %p(ptr null)
  ret i8 %v
}

define i8 @tgt(ptr %p) {
  %cmp = icmp eq ptr %p, @fn
  br i1 %cmp, label %then, label %else

then:
  %v = call i8 @fn(ptr null)
  ret i8 %v

else:
  %v2 = call i8 %p(ptr null)
  ret i8 %v2
}

declare i8 @fn(ptr captures(none))
