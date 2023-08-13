declare i32 @gettimeofday(ptr, ptr)

define i32 @f(ptr %tv, ptr %tz) {
  %res = call i32 @gettimeofday(ptr %tv, ptr %tz)
  ret i32 %res
}

; CHECK: nocapture
