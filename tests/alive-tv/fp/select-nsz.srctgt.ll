define float @src(float %x, float %y, i1 %b) {
  %ny = fneg float %y
  %s = select nsz i1 %b, float %x, float %ny
  %r = fneg float %s
  ret float %r
}

define float @tgt(float %x, float %y, i1 %b) {
  %x.neg = fneg nsz float %x
  %a = select nsz i1 %b, float %x.neg, float %y
  ret float %a
}
