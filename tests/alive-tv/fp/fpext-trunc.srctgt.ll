define half @src(half %0) {
  %2 = fpext half %0 to float
  %3 = fptrunc float %2 to half
  ret half %3
}

define half @tgt(half %0) {
  ret half %0
}

define half @src1(float noundef %a) {
  %b = fneg nnan float %a
  %c = fptrunc nnan float %b to half
  ret half %c
}

define half @tgt1(float noundef %a) {
  %b = fptrunc nnan float %a to half
  %c = fneg nnan half %b
  ret half %c
}

define half @src2(half noundef %a) {
  %av = fpext nnan half %a to float
  %c = fadd nnan float %av, %av
  %d = fptrunc nnan float %c to half
  ret half %d
}

define half @tgt2(half noundef %a) {
  %b = fadd nnan half %a, %a
  ret half %b
}
