target datalayout = "E-p:32:32:32-p1:16:16:16-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:32:64-f32:32:32-f64:64:64-n32"

define i8 @test1(ptr %p) {
  store i32 287454020, ptr %p ; 0x11223344
  ret i8 17
}

define i8 @test2(ptr %p) {
  store i32 287454020, ptr %p ; 0x11223344
  ret i8 34
}

define i8 @test3(ptr %p) {
  store i32 287454020, ptr %p ; 0x11223344
  ret i8 51
}

define i8 @test4(ptr %p) {
  store i32 287454020, ptr %p ; 0x11223344
  ret i8 68
}
