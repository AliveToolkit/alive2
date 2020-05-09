; TEST-ARGS: -dbg
target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

@c = constant [8 x i8] c"abcde\00f\00"

define i64 @src() {
  %p = bitcast [8 x i8]* @c to i8*
  %l = call i64 @strlen(i8* %p)
  ret i64 %l
}

define i64 @tgt() {
  %p = bitcast [8 x i8]* @c to i8*
  %l = call i64 @strlen(i8* %p)
  ret i64 %l
}

; CHECK: max_access_size: 8
declare i64 @strlen(i8*)
