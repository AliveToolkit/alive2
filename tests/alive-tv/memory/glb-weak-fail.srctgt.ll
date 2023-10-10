@g = weak constant i32 0

define i32 @src() {
  %x = load i32, ptr @g
  ret i32 %x
}

define i32 @tgt() {
  ret i32 0
}

; ERROR: Value mismatch
