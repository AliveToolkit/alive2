target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

@c = constant [9 x i8] c"abcde\00f\00g"

define i64 @f() {
  %p = bitcast [9 x i8]* @c to i8*
  %l = call i64 @strlen(i8* %p)
  ret i64 %l
}

; ERROR: Value mismatch
declare i64 @strlen(i8*)
