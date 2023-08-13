; not a bug after all

@Q = internal unnamed_addr global double 1.000000e+00, align 8

define double @src(i1 %c, ptr %p) {
entry:
  br i1 %c, label %if, label %end

if:
  %load = load i64, ptr bitcast (ptr @Q to ptr), align 8
  br label %end

end:
  %phi = phi i64 [ 0, %entry ], [ %load, %if ]
  store i64 %phi, ptr %p, align 8
  %cast = bitcast i64 %phi to double
  ret double %cast

  uselistorder i64 %phi, { 1, 0 }
}

define double @tgt(i1 %c, ptr %p) {
entry:
  br i1 %c, label %if, label %end

if:                                               ; preds = %entry
  %load1 = load double, ptr @Q, align 8
  br label %end

end:                                              ; preds = %if, %entry
  %0 = phi double [ 0.000000e+00, %entry ], [ %load1, %if ]
  store double %0, ptr %p, align 8
  ret double %0
}
