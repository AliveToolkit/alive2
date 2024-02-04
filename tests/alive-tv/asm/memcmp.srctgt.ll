; TEST-ARGS: -tgt-is-asm

define i32 @src(ptr, ptr) {
  %3 = call i32 @memcmp(ptr %0, ptr %1, i64 1)
  ret i32 %3
}

define i32 @tgt(ptr %0, ptr %1) {
  %lhsc = load i8, ptr %0, align 1
  %lhsv = zext i8 %lhsc to i32
  %rhsc = load i8, ptr %1, align 1
  %rhsv = zext i8 %rhsc to i32
  %chardiff = sub nsw i32 %lhsv, %rhsv
  ret i32 %chardiff
}

declare i32 @memcmp(ptr, ptr, i64)
