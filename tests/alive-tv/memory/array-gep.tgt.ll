target datalayout = "i24:32:32" ; 4-bytes aligned

define i24* @f([4 x i24]* %p) {
  %p2 = bitcast [4 x i24]* %p to i8*
  %q0 = getelementptr i8, i8* %p2, i32 4
  %q = bitcast i8* %q0 to i24*
  ret i24* %q
}
