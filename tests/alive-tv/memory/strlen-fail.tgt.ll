target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

@c = constant [9 x i8] c"abcde\00f\00g"

define i64 @f() {
  ret i64 6
}

declare i64 @strlen(i8*)
