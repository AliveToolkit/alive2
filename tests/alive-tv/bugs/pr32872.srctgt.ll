 ; https://bugs.llvm.org/show_bug.cgi?id=32872

 define <2 x float> @src(<2 x float> %v) {
   %tmp1 = shufflevector <2 x float> %v, <2 x float> zeroinitializer, <4 x i32> <i32 2, i32 2, i32 0, i32 1>
   %tmp4 = shufflevector <4 x float> zeroinitializer, <4 x float> %tmp1, <2 x i32> <i32 4, i32 5>
   ret <2 x float> %tmp4
 }

define <2 x float> @tgt(<2 x float> %v) {
  ret <2 x float> %v
}

; ERROR: Target is more poisonous than source
