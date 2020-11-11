; Found by Alive2

define <2 x i8> @src(<2 x i8> %a0) {
  %1 = ashr <2 x i8> < i8 251, i8 undef >, %a0
  %2 = xor <2 x i8> < i8 255, i8 255 >, %1
  ret <2 x i8> %2
}

define <2 x i8> @tgt(<2 x i8> %a0) {
  %1 = lshr <2 x i8> < i8 4, i8 undef >, %a0
  ret <2 x i8> %1
}

; ERROR: Value mismatch
