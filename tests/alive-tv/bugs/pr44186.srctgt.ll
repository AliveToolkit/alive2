; Found by Alive2

define <4 x i32> @src(<4 x i32> %a0) {
  %1 = urem <4 x i32> <i32 1, i32 1, i32 1, i32 undef>, %a0
  ret <4 x i32> %1
}

define <4 x i32> @tgt(<4 x i32> %a0) {
  %1 = icmp ne <4 x i32> %a0, <i32 1, i32 1, i32 1, i32 undef>
  %2 = zext <4 x i1> %1 to <4 x i32>
  ret <4 x i32> %2
}

; ERROR: Target's return value is more undefined
