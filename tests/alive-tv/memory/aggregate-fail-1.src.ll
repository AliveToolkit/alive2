target datalayout="e-p:64:64:64-p1:16:16:16-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64"

define i32 @test1(ptr %x) {
  ; {{ 0xDEADBEEF, 0xBA }, 0xCAFEBABE}
  store {{i32,i8},i32} {{i32,i8} { i32 -559038737, i8 186 }, i32 -889275714 }, ptr %x
  %y = getelementptr {{i32,i8},i32}, ptr %x, i32 0, i32 0, i32 0
  %r = load i32, ptr %y
  ret i32 %r
}

; ERROR: Value mismatch
