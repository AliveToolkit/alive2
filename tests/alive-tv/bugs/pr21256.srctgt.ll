; https://bugs.llvm.org/show_bug.cgi?id=21256

define i8 @src(i8 %X, i8 %Op0) {
  %Op1 = sub i8 0, %X
  %r = srem i8 %Op0, %Op1
  ret i8 %r
}

define i8 @tgt(i8 %X, i8 %Op0) {
  %r = srem i8 %Op0, %X
  ret i8 %r
}

; ERROR: Source is more defined than target
