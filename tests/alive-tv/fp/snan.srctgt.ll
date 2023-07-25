define i1 @src_a() {
  ; snan / 0.0 -> non-det(qnan, snan)
  %r = fdiv double 0x7ff0000000000001, 0.0
  %qnan = call i1 @llvm.is.fpclass(double %r, i32 1)
  ret i1 %qnan
}

define i1 @tgt_a() {
  ret i1 false
}

define i1 @src_b() {
  ; snan / 0.0 -> non-det(qnan, snan)
  %r = fdiv double 0x7ff0000000000001, 0.0
  %qnan = call i1 @llvm.is.fpclass(double %r, i32 1)
  ret i1 %qnan
}

define i1 @tgt_b() {
  ret i1 false
}

define i1 @src_a2() {
  ; snan / 0.0 -> non-det(qnan, snan)
  %r = fdiv double 0x7ff0000000000001, 0.0
  %qnan = call i1 @llvm.is.fpclass(double %r, i32 1)
  ret i1 %qnan
}

define i1 @tgt_a2() {
  ret i1 true
}

define i1 @src_b2() {
  ; snan / 0.0 -> non-det(qnan, snan)
  %r = fdiv double 0x7ff0000000000001, 0.0
  %qnan = call i1 @llvm.is.fpclass(double %r, i32 1)
  ret i1 %qnan
}

define i1 @tgt_b2() {
  ret i1 true
}

define i1 @src_c() {
  ; snan / 0.0 -> non-det(qnan, snan)
  %r = fdiv double 0x7ff0000000000001, 0.0
  %qnan = call i1 @llvm.is.fpclass(double %r, i32 3)
  ret i1 %qnan
}

define i1 @tgt_c() {
  ret i1 true
}

declare i1 @llvm.is.fpclass(double, i32)
