target datalayout = "e-i32:32-i128:32-i16:16"

%0 = type { i32, i128, i16 }

define i64 @0(%0*) {
  ret i64 24
}
