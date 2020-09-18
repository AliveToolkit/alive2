target triple = "x86_64-apple-macosx10.15.0"

define i32 @src() {
  ret i32 64
}

define i32 @tgt() {
  %v = call i32 @ffsll(i64 9223372036854775808)
  ret i32 %v
}

declare i32 @ffsll(i64)
