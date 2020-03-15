; https://bugs.llvm.org/show_bug.cgi?id=21256

define i32 @src(i32 %X, i32 %Op0) {
  %Op1 = sub i32 0, %X
  %r = srem i32 %Op0, %Op1
  ret i32 %r
}

define i32 @tgt(i32 %X, i32 %Op0) {
  %r = srem i32 %Op0, %X
  ret i32 %r
}

; ERROR: Source is more defined than target
