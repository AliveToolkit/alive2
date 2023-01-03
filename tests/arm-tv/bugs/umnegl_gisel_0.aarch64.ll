; TEST-ARGS: --disable-undef-input --disable-poison-input --global-isel

define i64 @test6_umsubl(i64 %0) {
  %2 = trunc i64 %0 to i32
  %3 = trunc i64 %0 to i32
  %4 = call i32 @llvm.smin.i32(i32 %2, i32 %3)
  %5 = zext i32 %4 to i64
  %6 = mul nsw i64 %5, 6
  %7 = sub i64 0, %6
  ret i64 %7
}

; Function Attrs: nocallback nofree nosync nounwind readnone speculatable willreturn
declare i32 @llvm.smin.i32(i32, i32) #0

attributes #0 = { nocallback nofree nosync nounwind readnone speculatable willreturn }