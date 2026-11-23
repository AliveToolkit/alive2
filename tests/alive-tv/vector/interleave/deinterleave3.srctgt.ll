; LangRef example: deinterleave3 of <0,1,2,3,4,5> = {<0,3>, <1,4>, <2,5>}

define { <2 x i32>, <2 x i32>, <2 x i32> } @src() {
  %d = call { <2 x i32>, <2 x i32>, <2 x i32> } @llvm.vector.deinterleave3.v6i32(<6 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5>)
  ret { <2 x i32>, <2 x i32>, <2 x i32> } %d
}

define { <2 x i32>, <2 x i32>, <2 x i32> } @tgt() {
  ret { <2 x i32>, <2 x i32>, <2 x i32> } { <2 x i32> <i32 0, i32 3>, <2 x i32> <i32 1, i32 4>, <2 x i32> <i32 2, i32 5> }
}
