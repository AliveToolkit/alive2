target datalayout = "e-i64:64"

@g = external global i64

define i64 @src() {
  %1 = load i64, ptr @g, align 16
  ret i64 %1
}

define i64 @tgt() {
  %1 = load i64, ptr @g
  ret i64 %1
}
