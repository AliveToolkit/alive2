target datalayout = "E-p:64:64:64"

define i32 @f1(i32* %p) {
  store i32 10, i32* %p, align 1
  %p8 = bitcast i32* %p to i8*
  %q8 = getelementptr i8, i8* %p8, i64 -1
  %q = bitcast i8* %q8 to i32*
  store i32 20, i32* %q, align 1
  ret i32 5130 ; 20 * 256 + 10
}
