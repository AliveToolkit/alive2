; target: 64 bits ptr addr
target datalayout = "p:64:64:64"

define i1 @disj() {
  %x = alloca i32
  %y = alloca i64
  %ix = ptrtoint ptr %x to i64
  %iy = ptrtoint ptr %y to i64
  %yx = sub i64 %iy, %ix ; %yx <= -8 \/ 4 <= %yx
  %left = icmp sle i64 %yx, -8
  %right = icmp sle i64 4, %yx
  %or = or i1 %left, %right
  ret i1 %or
}

; ERROR: Value mismatch
