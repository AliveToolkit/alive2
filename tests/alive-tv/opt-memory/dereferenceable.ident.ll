; TEST-ARGS: -dbg

define i32 @src(ptr dereferenceable(2) align 4 %p) {
  %v1 = load i32, ptr %p, align 4
  ret i32 %v1
}

define i32 @tgt(ptr dereferenceable(2) align 4 %p) {
  %v1 = load i32, ptr %p, align 4
  ret i32 %v1
}

; CHECK: min_access_size: 4
