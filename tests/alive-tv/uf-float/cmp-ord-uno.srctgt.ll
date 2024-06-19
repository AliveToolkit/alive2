; TEST-ARGS: --uf-float

define i1 @src(float noundef %x, float noundef %y) {
  %cmp = fcmp ord float %x, %y
  %notcmp = xor i1 %cmp, 1
  ret i1 %notcmp
}

define i1 @tgt(float noundef %x, float noundef %y) {
  %cmp = fcmp uno float %y, %x
  ret i1 %cmp
}