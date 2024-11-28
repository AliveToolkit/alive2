define i32 @src(i32 %a) {
  %poison = add nsw i32 2147483647, 100
  %v = insertelement <vscale x 2 x i32> undef, i32 %a, i32 0
  %v2 = insertelement <vscale x 2 x i32> %v, i32 %poison, i32 1
  %w = extractelement <vscale x 2 x i32> %v2, i32 0
  ret i32 %w
}

define i32 @tgt(i32 %a) {
  %poison = add nsw i32 2147483647, 100
  ret i32 %poison
}

; ERROR: Target is more poisonous than source
