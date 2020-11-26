; TEST-ARGS: -dbg

define i32 @src(i32* dereferenceable(2) %p) {
  %v1 = load i32, i32* %p, align 4
  ret i32 %v1
}

define i32 @tgt(i32* dereferenceable(2) %p) {
  %v1 = load i32, i32* %p, align 4
  ret i32 %v1
}

; TODO - since %p is dereferenced with a load with align 4, min_access_size can be
; up to 2.
; CHECK: min_access_size: 1
