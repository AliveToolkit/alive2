; TEST-ARGS: -dbg

define i32 @src(i32* dereferenceable(2) align 4 %p) {
  %v1 = load i32, i32* %p, align 4
  ret i32 %v1
}

define i32 @tgt(i32* dereferenceable(2) align 4 %p) {
  %v1 = load i32, i32* %p, align 4
  ret i32 %v1
}

; CHECK: min_access_size: 4
