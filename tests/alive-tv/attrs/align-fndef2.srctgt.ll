define align 4 ptr @src(ptr %p) {
  load i8, ptr %p
  ret ptr %p
}

define align 4 ptr @tgt(ptr %p) {
  load i8, ptr %p, align 4
  ret ptr %p
}

; Returning non-aligned pointer is poison, not UB
; ERROR: Source is more defined than target
