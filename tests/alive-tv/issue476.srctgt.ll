define i32 @src(i8* dereferenceable(4) %p) {
  ret i32 0
}

define i32 @tgt(i8* dereferenceable(8) %p) {
  ret i32 0
}

; ERROR: Unsupported interprocedural transformation: signature mismatch between src and tgt
