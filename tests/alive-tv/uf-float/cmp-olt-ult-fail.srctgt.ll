; TEST-ARGS: --uf-float
; ERROR: Couldn't prove the correctness of the transformation

define i1 @src(float noundef %x, float noundef %y) {
  %cmp = fcmp olt float %x, %y
  ret i1 %cmp
}

define i1 @tgt(float noundef %x, float noundef %y) {
  %cmp = fcmp ult float %x, %y
  ret i1 %cmp
}