; ERROR: Source is more defined than target

define i8 @src(ptr %p) {
  %v = call i8 @f(ptr %p)
  ret i8 %v
}

define i8 @tgt(ptr %p) {
  %v = call i8 @f(ptr readnone %p)
  ret i8 %v
}

declare i8 @f(ptr)
