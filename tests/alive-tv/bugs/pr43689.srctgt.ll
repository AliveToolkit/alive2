define <2 x i16> @src(i16 %a, i1 %cmp) {
  %splatinsert = insertelement <2 x i16> undef, i16 %a, i32 0
  %t1 = srem <2 x i16> %splatinsert, <i16 2, i16 1>
  %splat.op = shufflevector <2 x i16> %t1, <2 x i16> undef, <2 x i32> <i32 undef, i32 0>
  %t2 = select i1 %cmp, <2 x i16> <i16 77, i16 99>, <2 x i16> %splat.op
  ret <2 x i16> %t2
}

define <2 x i16> @tgt(i16 %a, i1 %cmp) {
  %1 = insertelement <2 x i16> undef, i16 %a, i32 1
  %2 = srem <2 x i16> %1, <i16 undef, i16 2>
  %t2 = select i1 %cmp, <2 x i16> <i16 77, i16 99>, <2 x i16> %2
  ret <2 x i16> %t2
}

; ERROR: Source is more defined than target
