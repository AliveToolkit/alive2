define i1 @src() {
  %val = call i1 @llvm.is.fpclass.f64(double 0x7FF8000000000000, i32 2)
  ret i1 %val
}

define i1 @tgt() {
  ret i1 true
}

declare i1 @llvm.is.fpclass.f64(double, i32 immarg)
