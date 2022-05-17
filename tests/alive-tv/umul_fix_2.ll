declare i35 @llvm.umul.fix.i35(i35, i35, i32)

define i35 @umul_fix(i35 %x) {
  %r = call i35 @llvm.umul.fix.i35(i35 42, i35 %x, i32 2)
  ret i35 %r
}
