define i1 @src() {
  %a = alloca i64
  %b = alloca i64
  %cmp = icmp eq i64* %a, %b
  ret i1 %cmp
}
define i1 @tgt() {
  ret i1 0
}

