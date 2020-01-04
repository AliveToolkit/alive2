; TEST-ARGS: -dbg
target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

@c = constant [15 x i8] c"abcde\00f\00gh12345"

define i64 @f() {
  %p = bitcast [15 x i8]* @c to i8*
  %l = call i64 @strlen(i8* %p)
  ret i64 %l
}
; CHECK: max_mem_access: 15
declare i64 @strlen(i8*)
