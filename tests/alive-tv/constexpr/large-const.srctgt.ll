@str = constant [129 x i8] c"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", align 16

define i8 @src(i32 %idx) {
  %arrayidx = getelementptr inbounds [129 x i8], ptr @str, i64 0, i32 %idx
  %v = load i8, ptr %arrayidx, align 1
  ret i8 %v
}

define i8 @tgt(i32 %idx) {
  %idxp = sext i32 %idx to i64
  %arrayidx = getelementptr inbounds [129 x i8], ptr @str, i64 0, i64 %idxp
  %v = load i8, ptr %arrayidx, align 1
  ret i8 %v
}
