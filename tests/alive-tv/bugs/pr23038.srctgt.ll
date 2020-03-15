; https://bugs.llvm.org/show_bug.cgi?id=23038

define i32 @src(i8 %call) {
  %conv1 = sitofp i8 %call to float
  %mul = fmul float %conv1, -3.000000e+00
  %conv2 = fptosi float %mul to i8
  %conv3 = sext i8 %conv2 to i32
  ret i32 %conv3
}

define i32 @tgt(i8 %call) {
  %1 = sext i8 %call to i32
  %mul1 = mul i32 %1, 0  ;; XXX nope
  %2 = trunc i32 %mul1 to i8
  %conv3 = sext i8 %2 to i32
  ret i32 %conv3
}

; ERROR: Value mismatch
