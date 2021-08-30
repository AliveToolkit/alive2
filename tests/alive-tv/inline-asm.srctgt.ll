define i32 @src(i32 %Y, i1 %c) {
  br i1 %c, label %then, label %else

then:
  %X = call i32 asm "bswap $0", "=r,r"(i32 %Y)
  ret i32 %X

else:
  %Z = call i32 asm "bswap $0", "=r,r"(i32 %Y)
  ret i32 %Z
}

define i32 @tgt(i32 %Y, i1 %c) {
  %X = call i32 asm "bswap $0", "=r,r"(i32 %Y)
  ret i32 %X
}
