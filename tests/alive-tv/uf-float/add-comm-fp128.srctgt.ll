; TEST-ARGS: --uf-float

define fp128 @src(fp128 noundef %x, fp128 noundef %y) {
  %sum = fadd fp128 %x, %y
  ret fp128 %sum
}

define fp128 @tgt(fp128 noundef %x, fp128 noundef %y) {
  %sum = fadd fp128 %y, %x
  ret fp128 %sum
}