; ERROR: Source is more defined than target

declare void @foo_i8(i8)

define void @src(i8 %x, i8 %y) {
  %cond.fr = freeze i1 undef
  %s = select i1 %cond.fr, i8 %x, i8 0
  %s2 = select i1 %cond.fr, i8 0, i8 %x
  call void @foo_i8(i8 %s)
  call void @foo_i8(i8 %s2)
  ret void
}

define void @tgt(i8 %x, i8 %y) {
  call void @foo_i8(i8 0)
  call void @foo_i8(i8 0)
  ret void
}
