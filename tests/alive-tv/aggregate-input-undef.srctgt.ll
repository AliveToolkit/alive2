; TEST-ARGS: -disable-poison-input
; ERROR: Target's return value is more undefined

; Include an empty aggregate and alignment padding before the nested array.
%T = type { {}, i8, [2 x <2 x i32>] }

define i32 @src(%T %x) {
  %a = extractvalue %T %x, 2, 0
  %v = extractelement <2 x i32> %a, i32 1
  ret i32 %v
}

define i32 @tgt(%T %x) {
  %a = extractvalue %T %x, 2, 1
  %v = extractelement <2 x i32> %a, i32 0
  ret i32 %v
}
