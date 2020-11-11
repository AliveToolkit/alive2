; Found by Alive2

define <2 x i1> @src(<2 x i8> %a) {
  %cmp = icmp sle <2 x i8> %a, <i8 undef, i8 0>
  ret <2 x i1> %cmp
}

define <2 x i1> @tgt(<2 x i8> %a) {
  %cmp = icmp slt <2 x i8> %a, <i8 undef, i8 1>
  ret <2 x i1> %cmp
}

; ERROR: Target's return value is more undefined
