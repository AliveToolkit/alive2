; ERROR: Source is more defined than target

define i8 @src(ptr %p, ptr %fn) {
  %v = call i8 %fn(ptr %p)
  ret i8 %v
}

define i8 @tgt(ptr %p, ptr %fn) {
  %c = icmp eq ptr %fn, @f
  br i1 %c, label %then, label %else
then:
  %v = call i8 @f(ptr readnone %p)
  ret i8 %v

else:
  %v2 = call i8 %fn(ptr %p)
  ret i8 %v2
}

declare i8 @f(ptr)
