; target: 64 bits ptr addr
target datalayout = "p:64:64:64"
@x = global i32 0

define i1 @disj() {
  %y = alloca i64
  %ix = ptrtoint ptr @x to i64
  %iy = ptrtoint ptr %y to i64
  %yx = sub i64 %iy, %ix ; %yx <= -8 \/ 4 <= %yx
  %left = icmp sle i64 %yx, -8
  %right = icmp sle i64 4, %yx
  %or = or i1 %left, %right
  ret i1 %or
}

define i1 @disj_false() {
  %y = alloca i64
  %ix = ptrtoint ptr @x to i64
  %iy = ptrtoint ptr %y to i64
  %yx = sub i64 %iy, %ix ; %yx <= -8 \/ 4 <= %yx
  %left = icmp slt i64 -8, %yx
  %right = icmp slt i64 %yx, 4
  %and = and i1 %left, %right
  ret i1 %and
}
