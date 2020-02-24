target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

@g = global [8 x i8] undef

define i1 @src() {
  %p = bitcast [8 x i8]* @g to i8*
  %l = call i64 @strlen(i8* %p)
  %c = icmp eq i64 %l, 1
  ret i1 %c
}

define i1 @tgt() {
  %p = bitcast [8 x i8]* @g to i8*
  %v = load i8, i8* %p
  %c = icmp eq i8 %v, 0
  ret i1 %c
}

; ERROR: Value mismatch
declare i64 @strlen(i8*)
