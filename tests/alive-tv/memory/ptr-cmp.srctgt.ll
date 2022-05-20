define i1 @src(ptr %base, i64 %idx) {
  %gep = getelementptr inbounds i8, ptr %base, i64 %idx
  %cnd = icmp eq ptr %gep, null
  %A = alloca i1, i32 0, align 1
  ret i1 %cnd
}

define i1 @tgt(ptr %base, i64 %idx) {
  %cnd = icmp eq ptr %base, null
  ret i1 %cnd
}
