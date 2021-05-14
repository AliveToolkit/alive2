define double @src(double %x) {
  %lezero = fcmp ole double %x, 0.000000
  %negx = fsub nnan double 0.000000, %x
  %fabs = select nnan i1 %lezero, double %negx, double %x
  ret double %fabs
}

define double @tgt(double %x) {
  %fabs = call nnan double @llvm.fabs.f64(double %x)
  ret double %fabs
}

declare double @llvm.fabs.f64(double)
