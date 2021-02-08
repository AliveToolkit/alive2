%struct.timeval = type { i64, i64 }
%struct.timezone = type { i32, i32 }

declare dso_local i32 @gettimeofday(%struct.timeval*, i8*)

define i32 @f(%struct.timeval* %tv, i8* %tz) {
  %res = call i32 @gettimeofday(%struct.timeval* %tv, i8* %tz)
  ret i32 %res
}

; CHECK: nocapture
