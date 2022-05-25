define float @src(float %a, float %b, float %c) {
entry:
  %t1 = fcmp oeq float %b, 0.0
  br i1 %t1, label %bb1, label %bb3

bb1:
  %t2 = fcmp ogt float %c, %c
  br i1 %t2, label %bb2, label %bb3

bb2:
  br label %bb3

bb3:
  %t4 = phi nsz float [ %a, %entry ], [ %b, %bb1 ], [ %c, %bb2 ]
  ret float %t4
}

define float @tgt(float %a, float %b, float %c) {
  %t1 = fcmp oeq float %b, 0.000000
  %t4 = select nsz i1 %t1, float %b, float %a
  ret float %t4
}
