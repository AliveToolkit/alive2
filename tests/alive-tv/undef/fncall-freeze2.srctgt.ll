; ERROR: Value mismatch

declare i8 @foo_i8(i8) readonly

define i8 @src(i8 %x, i8 %y) {
  %cond.fr = freeze i1 undef
  %s = select i1 %cond.fr, i8 %x, i8 0
  %s2 = select i1 %cond.fr, i8 0, i8 %x
  %a = call i8 @foo_i8(i8 %s)
  %b = call i8 @foo_i8(i8 %s2)
  %c = add i8 %a, %b
  ret i8 %c
}

define i8 @tgt(i8 %x, i8 %y) {
  %a = call i8 @foo_i8(i8 0)
  %b = call i8 @foo_i8(i8 0)
  %c = add i8 %a, %b
  ret i8 %c
}
