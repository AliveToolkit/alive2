define i8 @src(ptr %p) memory(argmem: readwrite) {
  %p2 = getelementptr i8, ptr %p, i32 0
  %v = call i8 @f(ptr %p2)
  ret i8 %v
}

define i8 @tgt(ptr %p) memory(argmem: readwrite) {
  %v = call i8 @f(ptr %p)
  ret i8 %v
}

declare i8 @f(ptr) memory(argmem: readwrite)
