; target: 64 bits ptr addr
target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i8 @alloca_malloc() {
  %ptr = alloca i8
  store i8 20, ptr %ptr
  %v = load i8, ptr %ptr
  ret i8 %v
}
