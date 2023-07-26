; Found by Alive2
; SKIP-IDENTITY

target datalayout = "p:64:64:64-i64:32:32"

define ptr @src(ptr %x) {
entry:
  %b1 = load i64, ptr %x
  %p2 = inttoptr i64 %b1 to ptr
  ret ptr %p2
}

define ptr @tgt(ptr %x) {
entry:
  %b11 = load ptr, ptr %x, align 8
  ret ptr %b11
}

; ERROR: Source is more defined than target
