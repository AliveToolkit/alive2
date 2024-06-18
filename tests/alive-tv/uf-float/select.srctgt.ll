; TEST-ARGS: --uf-float

define float @src(float noundef %x, float noundef %y) {
  %cmp = fcmp olt float %x, %y
  %a = fsub float %y, %x
  %b = fsub float %x, %y
  %c = select i1 %cmp, float %a, float %b
  ret float %c
}

define float @tgt(float noundef %x, float noundef %y) {
  %cmp = fcmp olt float %x, %y
  %min = select i1 %cmp, float %x, float %y
  %max = select i1 %cmp, float %y, float %x
  %res = fsub float %max, %min
  ret float %res
}