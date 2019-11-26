target datalayout = "e-i32:32-i128:32-i16:16"

%0 = type { i32, i128, i16 }

define i64 @0(%0*) {
  %2 = getelementptr inbounds %0, %0* %0, i32 0, i32 2
  %3 = ptrtoint %0* %0 to i64
  %4 = ptrtoint i16* %2 to i64
  %5 = sub i64 %4, %3
  ret i64 %5
}
