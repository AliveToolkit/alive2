target datalayout = "E-p:32:32:32-p1:16:16:16-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:32:64-f32:32:32-f64:64:64-n32"

define i8 @test1(ptr %p) {
  store i32 287454020, ptr %p ; 0x11223344
  %x = load i8, ptr %p
  ret i8 %x
}

define i8 @test2(ptr %p) {
  store i32 287454020, ptr %p ; 0x11223344
  %q2 = getelementptr inbounds i8, ptr %p, i64 1
  %x = load i8, ptr %q2
  ret i8 %x
}

define i8 @test3(ptr %p) {
  store i32 287454020, ptr %p ; 0x11223344
  %q2 = getelementptr inbounds i8, ptr %p, i64 2
  %x = load i8, ptr %q2
  ret i8 %x
}

define i8 @test4(ptr %p) {
  store i32 287454020, ptr %p ; 0x11223344
  %q2 = getelementptr inbounds i8, ptr %p, i64 3
  %x = load i8, ptr %q2
  ret i8 %x
}
