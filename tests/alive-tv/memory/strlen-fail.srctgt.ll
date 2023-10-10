target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

@c = constant [6 x i8] c"abcde\00"

define i64 @src() {
  %l = call i64 @strlen(ptr @c)
  ret i64 %l
}

define i64 @tgt() {
  ret i64 6
}

; ERROR: Value mismatch
declare i64 @strlen(ptr)
