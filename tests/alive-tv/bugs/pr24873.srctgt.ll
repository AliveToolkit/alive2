; https://bugs.llvm.org/show_bug.cgi?id=24873

define i1 @src(i64 %V) {
  %ashr = ashr i64 -4611686018427387904, %V
  %icmp = icmp eq i64 %ashr, -1
  ret i1 %icmp
}

define i1 @tgt(i64 %V) {
  %icmp = icmp eq i64 %V, 62
  ret i1 %icmp
}

; ERROR: Value mismatch
