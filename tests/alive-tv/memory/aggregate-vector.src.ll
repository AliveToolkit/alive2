; From Transforms/ConstProp/loads.ll
target datalayout="e-p:64:64:64-p1:16:16:16-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64"
@g = constant {i64, i64} { i64 123, i64 112312312 }

define <2 x i64> @test() {
  %r = load <2 x i64>, ptr @g
  ret <2 x i64> %r
}
