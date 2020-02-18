target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

@c = constant [4 x i8] c"a\00\00c"

define i64 @f() {
  %p = bitcast [4 x i8]* @c to i8*
  %l = call i64 @strlen(i8* %p)
  ret i64 %l
}

define i64 @f2() {
  %p0 = bitcast [4 x i8]* @c to i8*
  %p = getelementptr i8, i8* %p0, i32 2
  %l = call i64 @strlen(i8* %p)
  ret i64 %l
}

define i64 @f3() {
  %p0 = bitcast [4 x i8]* @c to i8*
  %p = getelementptr i8, i8* %p0, i32 3 ; UB
  %l = call i64 @strlen(i8* %p)
  ret i64 %l
}

declare i64 @strlen(i8*)
