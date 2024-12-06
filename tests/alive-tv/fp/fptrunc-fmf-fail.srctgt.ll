define half @src(float noundef %a) {
  %b = fneg ninf float %a
  %c = fptrunc float %b to half
  ret half %c
}

define half @tgt(float noundef %a) {
  %b = fptrunc ninf float %a to half
  %c = fneg ninf half %b
  ret half %c
}

; ERROR: Target is more poisonous than source
