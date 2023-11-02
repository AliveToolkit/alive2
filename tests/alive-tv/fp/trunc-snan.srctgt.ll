; ERROR: Value mismatch

define i1 @src() {
  %f = fptrunc double 0x7FF0100000000000 to float
  %d = fpext float %f to double
  %isnan = call i1 @llvm.is.fpclass(double %d, i32 3)
  ret i1 %isnan
}

define i1 @tgt() {
  ret i1 false
}

declare i1 @llvm.is.fpclass(double, i32)
