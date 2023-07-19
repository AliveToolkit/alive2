define i1 @src(ptr %x) {
  %y = load <2 x i8>, ptr %x, !range !1
  %v = extractelement <2 x i8> %y, i8 0
  %r = icmp ule i8 %v, 9
  ret i1 %r
}

define i1 @tgt(ptr %x) {
  ret i1 true
}

!1 = !{i8 0, i8 10}
