@x = global i32 1, align 4
@y = global i64 2, align 4

define i1 @src() {
  %q = getelementptr i32, ptr @y, i64 1
  %c = icmp eq ptr %q, @x
  ret i1 %c
}

define i1 @tgt() {
  ret i1 false
}
