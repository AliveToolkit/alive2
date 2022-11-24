define void @src() {
  ret void

bogusBB:
  %inc1 = fadd double %inc, 1.0
  %inc = fadd double %inc1, 1.0
  br label %bogusBB
}

define void @tgt() {
  ret void
}
