target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

@c = constant [2 x i8] c"a\00"

define i64 @f() {
  ret i64 0
}

declare i64 @strlen(i8*)
