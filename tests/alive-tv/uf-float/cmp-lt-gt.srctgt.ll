; TEST-ARGS: --uf-float

define i1 @src(float noundef %x, float noundef %y) {
  %cmp = fcmp olt float %x, %y
  ret i1 %cmp
}

define i1 @tgt(float noundef %x, float noundef %y) {
  %cmp = fcmp ogt float %y, %x
  ret i1 %cmp
}