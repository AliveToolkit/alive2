declare void @func3()

define void @src(i1 %c) {
  %fn = select i1 %c, ptr null, ptr @func3
  call void %fn(i8 0)
  ret void
}

define void @tgt(i1 %c) {
  unreachable
}
