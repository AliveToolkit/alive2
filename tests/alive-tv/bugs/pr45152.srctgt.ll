; not a bug after all

@Q = internal unnamed_addr global double 1.000000e+00, align 8

define double @src(i1 %c, i64* %p) {
entry:
  br i1 %c, label %if, label %end

if:
  %load = load i64, i64* bitcast (double* @Q to i64*), align 8
  br label %end

end:
  %phi = phi i64 [ 0, %entry ], [ %load, %if ]
  store i64 %phi, i64* %p, align 8
  %cast = bitcast i64 %phi to double
  ret double %cast

  uselistorder i64 %phi, { 1, 0 }
}

define double @tgt(i1 %c, i64* %p) {
entry:
  br i1 %c, label %if, label %end

if:                                               ; preds = %entry
  %load1 = load double, double* @Q, align 8
  br label %end

end:                                              ; preds = %if, %entry
  %0 = phi double [ 0.000000e+00, %entry ], [ %load1, %if ]
  %1 = bitcast i64* %p to double*
  store double %0, double* %1, align 8
  ret double %0
}
