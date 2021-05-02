define i1 @src(i64* %arg) {
  %alloc = alloca i64
  %cmp = icmp eq i64* %arg, %alloc
  ret i1 %cmp
}

define i1 @tgt(i64* %arg) {
  ret i1 false
}
