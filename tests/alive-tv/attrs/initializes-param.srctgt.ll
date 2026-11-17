; The initializes attribute on a parameter must be kept, not skipped.
; CHECK: define i32 @src(ptr initializes((0, 4)) %p)
define i32 @src(ptr initializes((0, 4)) %p) {
  store i32 0, ptr %p
  %v = load i32, ptr %p
  ret i32 %v
}

define i32 @tgt(ptr initializes((0, 4)) %p) {
  store i32 0, ptr %p
  ret i32 0
}
