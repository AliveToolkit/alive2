target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

@c = constant [8 x i8] c"abc\00de\00f"

define i64 @f() {
  ret i64 3
}

define i64 @f2() {
  ret i64 2
}

define i64 @f3() {
  unreachable
}

define i64 @f4(ptr %p, ptr %q) {
  %l2 = call i64 @strlen(ptr %p)
  %l1 = call i64 @strlen(ptr %p)
  %l = add i64 %l1, %l2
  ret i64 %l
}

declare i64 @strlen(ptr)
