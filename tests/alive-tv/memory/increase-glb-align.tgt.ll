@global = external global [50 x i8], align 8

define i32 @test() {
  %v = load i32, ptr @global, align 4
  ret i32 %v
}
