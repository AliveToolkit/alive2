define void @f() {
entry:
  ret void

dead:
  %v = load <4 x i16>, ptr null
  %blend = shufflevector <4 x i16> <i16 extractelement (<2 x i16> bitcast (<1 x float> splat (float 1.0) to <2 x i16>), i32 0), i16 undef, i16 undef, i16 undef>, <4 x i16> %v, <4 x i32> <i32 0, i32 4, i32 5, i32 6>
  store <4 x i16> %blend, ptr null
  br label %dead
}
