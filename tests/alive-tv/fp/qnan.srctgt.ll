define i1 @src(float %a) {
  %r = fdiv float 0.0, 0.0
  %qnan = call i1 @llvm.is.fpclass(float %r, i32 2)
  ret i1 %qnan
}

define i1 @tgt(float %a) {
  ret i1 true
}

declare i1 @llvm.is.fpclass(float, i32)
