; TEST-ARGS: -tgt-is-asm

define i1 @src(ptr %0, ptr %1) {
  %3 = getelementptr i8, ptr %0, i32 1
  %4 = getelementptr i8, ptr %1, i32 1
  %5 = icmp eq ptr %3, %4
  ret i1 %5
}

define i1 @tgt(ptr %0, ptr %1) {
  %r = icmp eq ptr %0, %1
  ret i1 %r
}
