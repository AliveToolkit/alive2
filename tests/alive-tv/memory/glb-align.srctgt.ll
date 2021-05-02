@x = global i32 1, align 4
@y = global i64 2, align 4

define i1 @src() {
  %p = bitcast i64* @y to i32*
  %q = getelementptr i32, i32* %p, i64 1
  %c = icmp eq i32* %q, @x
  ret i1 %c
}

define i1 @tgt() {
  ret i1 false
}
