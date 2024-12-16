; TEST-ARGS: --uf-float
; ERROR: Couldn't prove the correctness of the transformation

define float @src(float noundef %x, float noundef %y, float noundef %z) {
  %a = fadd float %x, %y
  %b = fadd float %a, %z
  ret float %b
}

define float @tgt(float noundef %x, float noundef %y, float noundef %z) {
  %a = fadd float %y, %z
  %b = fadd float %x, %a
  ret float %b
}