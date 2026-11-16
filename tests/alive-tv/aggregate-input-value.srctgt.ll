; ERROR: Value mismatch

define i32 @src({ <2 x i32>, <2 x i32> } noundef %x) {
  %a = extractvalue { <2 x i32>, <2 x i32> } %x, 0
  %v = extractelement <2 x i32> %a, i32 1
  ret i32 %v
}

define i32 @tgt({ <2 x i32>, <2 x i32> } noundef %x) {
  %a = extractvalue { <2 x i32>, <2 x i32> } %x, 1
  %v = extractelement <2 x i32> %a, i32 0
  ret i32 %v
}
