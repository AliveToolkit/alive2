target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

@c = constant [9 x i8] c"abcde\00f\00g"

define i64 @f() {
  ret i64 5
}

define i64 @f2() {
  ret i64 3
}

define i64 @f3() {
  ret i64 0
}

define i64 @f4() {
  ret i64 1
}

define i64 @f5() {
  unreachable
}

declare i64 @strlen(i8*)
