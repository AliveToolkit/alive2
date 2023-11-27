; TEST-ARGS: -tgt-is-asm

define <1 x i8> @src(ptr %0) {
  %2 = load <1 x i8>, ptr %0, align 4
  ret <1 x i8> %2
}

define <1 x i8> @tgt(ptr %0) {
  %a2_11 = load <1 x i8>, ptr %0, align 2
  ret <1 x i8> %a2_11
}
