target datalayout = "E-p:64:64:64"

define i32 @f1(i64 %x, i64 %y) {
  %lt = icmp ult i64 %x, %y
  %eq = icmp eq i64 %x, %y
  %r = select i1 %eq, i32 0, i32 1
  %res = select i1 %lt, i32 -1, i32 %r
  ret i32 %res
}

declare i32 @memcmp(i8* nocapture, i8* nocapture, i64)

