target datalayout = "i24:32:32" ; 4-bytes aligned

define i24* @f([4 x i24]* %p) {
  %q = getelementptr [4 x i24], [4 x i24]* %p, i32 0, i32 1
  ret i24* %q
}
