define i32 @src() {
  ret i32 undef
}

define i32 @tgt() {
  ret i32 poison
}

; ERROR: Target is more poisonous than source
