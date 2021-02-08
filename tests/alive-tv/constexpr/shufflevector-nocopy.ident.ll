; Check whether this example does not cause crash
define <4 x i32> @f(<4 x i32> %x, <4 x i32> %y) {
  %z = shufflevector <4 x i32> %x, <4 x i32> %y, <4 x i32> <i32 0, i32 1, i32 undef, i32 3>
  ret <4 x i32> %z
}

