@glb = global i8 0

define i1 @src(i64 %idx) {
  %p = getelementptr inbounds i8, ptr @glb, i64 %idx
  %c = icmp eq ptr %p, null
  ret i1 %c
}

define i1 @tgt(i64 %idx) {
  ret i1 false
}
