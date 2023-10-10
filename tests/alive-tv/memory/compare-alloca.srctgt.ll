define i1 @src(ptr %arg) {
  %alloc = alloca i64
  %cmp = icmp eq ptr %arg, %alloc
  ret i1 %cmp
}

define i1 @tgt(ptr %arg) {
  ret i1 false
}
