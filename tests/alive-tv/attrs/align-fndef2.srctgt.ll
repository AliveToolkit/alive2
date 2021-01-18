define align 4 i8* @src(i8* %p) {
  load i8, i8* %p
  ret i8* %p
}

define align 4 i8* @tgt(i8* %p) {
  load i8, i8* %p, align 4
  ret i8* %p
}

; Returning non-aligned pointer is poison, not UB
; ERROR: Source is more defined than target
