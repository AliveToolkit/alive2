target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

@c = constant [4 x i8] c"a\00\00c"

define i64 @f() {
  ret i64 1
}

define i64 @f2() {
  ret i64 0
}

define i64 @f3() {
  unreachable
}


declare i64 @strlen(i8*)
