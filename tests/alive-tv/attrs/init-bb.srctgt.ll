define half @src(i8 range(i8 0, 8) %num) {
entry:
  %conv1 = uitofp i8 %num to half
  %div = fdiv half %conv1, 1.000000e+01
  ret half %div
}

define half @tgt(i8 %num) {
entry:
  %conv1 = uitofp i8 %num to half
  %div = fdiv half 1.0, 1.000000e+01
  %mul = fmul half %conv1, %div
  ret half %mul
}

; CHECK-NOT: UB triggered on br
