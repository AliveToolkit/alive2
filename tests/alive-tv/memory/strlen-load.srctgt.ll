target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

@g = global [8 x i8] poison

define i1 @src() {
  %l = call i64 @strlen(ptr @g)
  %c = icmp eq i64 %l, 0
  ret i1 %c
}

define i1 @tgt() {
  %v = load i8, ptr @g
  %c = icmp eq i8 %v, 0
  ret i1 %c
}

declare i64 @strlen(ptr)
