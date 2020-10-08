; ERROR: Source is more defined than target

define i8 @src(i8 %x) {
  %freeze = freeze i8 undef
  %r = call i8 @f(i8 %freeze)
  ret i8 %r
}

define i8 @tgt(i8 %x) {
  %r = call i8 @f(i8 undef)
  ret i8 %r
}

declare i8 @f(i8)
