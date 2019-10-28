target datalayout = "E-p:32:32:32-p1:16:16:16-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:32:64-f32:32:32-f64:64:64-n32"

define i8 @test1(i32* %p) {
  %q = bitcast i32* %p to i8*
  store i32 287454020, i32* %p ; 0x11223344
  ret i8 68
}

; ERROR: Value mismatch
