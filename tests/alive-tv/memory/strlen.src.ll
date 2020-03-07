target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

@c = constant [8 x i8] c"abc\00de\00f"

define i64 @f() {
  %p = bitcast [8 x i8]* @c to i8*
  %l = call i64 @strlen(i8* %p)
  ret i64 %l
}

define i64 @f2() {
  %p0 = bitcast [8 x i8]* @c to i8*
  %p = getelementptr i8, i8* %p0, i32 4
  %l = call i64 @strlen(i8* %p)
  ret i64 %l
}

define i64 @f3() {
  %p0 = bitcast [8 x i8]* @c to i8*
  %p = getelementptr i8, i8* %p0, i32 7 ; UB
  %l = call i64 @strlen(i8* %p)
  ret i64 %l
}

define i64 @f4(i8* %p, i8* %q) {
  %l1 = call i64 @strlen(i8* %p)
  %l2 = call i64 @strlen(i8* %p)
  %l = add i64 %l1, %l2
  ret i64 %l
}

declare i64 @strlen(i8*)
