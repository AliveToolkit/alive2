; TEST-ARGS: --global-isel

define i64 @f(i64 %0, i1 %1) {
  %3 = zext i1 %1 to i32
  %4 = udiv exact i32 %3, -142620694
  %5 = zext i32 %4 to i64
  ret i64 %5
}
