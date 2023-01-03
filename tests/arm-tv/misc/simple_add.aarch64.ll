; ModuleID = 'simple_add.c'
source_filename = "simple_add.c"
target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64"

define i32 @add(i32 %a, i32 %b) {
entry:
  %add = add i32 %b, %a
  ret i32 %add
}
