; ERROR: Source is more defined

@constarray = external constant [8 x i8], align 4

define i32 @src(i64 %idx) {
  %1 = getelementptr i32, ptr @constarray, i64 %idx
  %2 = load i32, ptr %1, align 8
  ret i32 %2
}


define i32 @tgt(i64 %idx) {
  unreachable
}

