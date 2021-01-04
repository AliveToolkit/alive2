; FIXME: this should work some day..

; ERROR: Couldn't prove the correctness of the transformation

@str = constant [129 x i8] c"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", align 16

define i8 @src(i32 %idx) {
  %arrayidx = getelementptr inbounds [129 x i8], [129 x i8]* @str, i64 0, i32 %idx
  %v = load i8, i8* %arrayidx, align 1
  ret i8 %v
}

define i8 @tgt(i32 %idx) {
  ret i8 97
}
