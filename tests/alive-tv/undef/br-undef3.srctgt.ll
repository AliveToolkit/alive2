define i8 @src() {
  %f = freeze i1 undef
  br i1 %f, label %a, label %b

a:
  ret i8 0

b:
  ret i8 3
}

define i8 @tgt() {
  unreachable
}

; ERROR: Source is more defined
