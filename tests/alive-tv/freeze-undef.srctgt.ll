define i32 @src() {
  %v = freeze i32 undef
  ret i32 %v
}

define i32 @tgt() {
  ret i32 undef
}

; ERROR: Target's return value is more undefined
