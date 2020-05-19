target datalayout = "e-p:64:64:64"

define i32 @ub_null() {
  unreachable
}

define i32 @ub_oob() {
  unreachable
}

define i32 @ub_oob2() {
  unreachable
}

declare i32 @memcmp(i8* nocapture, i8* nocapture, i64)
