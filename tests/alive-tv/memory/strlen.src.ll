target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

@c = constant [8 x i8] c"abc\00de\00f"

define i64 @f() {
  %l = call i64 @strlen(ptr @c)
  ret i64 %l
}

define i64 @f2() {
  %p = getelementptr i8, ptr @c, i32 4
  %l = call i64 @strlen(ptr %p)
  ret i64 %l
}

define i64 @f3() {
  %p = getelementptr i8, ptr @c, i32 7 ; UB
  %l = call i64 @strlen(ptr %p)
  ret i64 %l
}

define i64 @f4(ptr %p, ptr %q) {
  %l1 = call i64 @strlen(ptr %p)
  %l2 = call i64 @strlen(ptr %p)
  %l = add i64 %l1, %l2
  ret i64 %l
}

declare i64 @strlen(ptr)
