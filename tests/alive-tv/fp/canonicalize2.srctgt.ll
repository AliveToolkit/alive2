; XFAIL: Value mismatch
; TODO: only works in some strict FP mode

define i1 @src(float %x) {
  %y = call float @llvm.canonicalize(float %x)
  %quiet = call i1 @llvm.is.fpclass(float %y, i32 2)
  %isnan = fcmp une float %x, %x
  %not_nan = xor i1 %isnan, 1
  %or = or i1 %not_nan, %quiet
  ret i1 %or
}

define i1 @tgt(float %a) {
  ret i1 true
}

declare float @llvm.canonicalize(float)
declare i1 @llvm.is.fpclass(float, i32)
