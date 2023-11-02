target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128" ; has little endian
@x = constant i32 257, align 4
@y = constant i32 258, align 4

define i8 @f() {
  %v = load i8, ptr @x
  %w = load i8, ptr @y
  %z = add i8 %v, %w
  ret i8 %z
}
