define i32 @src(i32 %a, i32 %n) {
  %vla = alloca i32, i32 %n, align 16
  %arrayidx = getelementptr inbounds i32, ptr %vla, i64 2
  store i32 3, ptr %arrayidx, align 8
  %arrayidx1 = getelementptr inbounds i32, ptr %vla, i32 %a
  %r = load i32, ptr %arrayidx1, align 4
  ret i32 %r
}

define i32 @tgt(i32 %a, i32 %n) {
  ret i32 3
}

