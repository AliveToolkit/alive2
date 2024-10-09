; TEST-ARGS: --uf-float

define double @src(double noundef %x, double noundef %y) {
  %sum = fadd double %x, %y
  ret double %sum
}

define double @tgt(double noundef %x, double noundef %y) {
  %sum = fadd double %y, %x
  ret double %sum
}