target triple = "x86_64-apple-macosx10.15.0"

define i32 @src() {
  %v = call i32 @ffs(i32 2147483648)
  ret i32 %v
}

define i32 @tgt() {
  ret i32 32
}

declare i32 @ffs(i32)
