target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i8 @calloc_overflow(i64 %num) {
  ret i8 1
}

