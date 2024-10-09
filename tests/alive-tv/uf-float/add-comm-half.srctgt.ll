; TEST-ARGS: --uf-float

define half @src(half noundef %x, half noundef %y) {
  %sum = fadd half %x, %y
  ret half %sum
}

define half @tgt(half noundef %x, half noundef %y) {
  %sum = fadd half %y, %x
  ret half %sum
}