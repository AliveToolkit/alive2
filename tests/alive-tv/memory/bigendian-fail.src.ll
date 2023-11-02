target datalayout = "E-p:32:32:32-p1:16:16:16-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:32:64-f32:32:32-f64:64:64-n32"

define i8 @test1(ptr %p) {
  store i32 287454020, ptr %p ; 0x11223344
  ret i8 68
}

; ERROR: Value mismatch
