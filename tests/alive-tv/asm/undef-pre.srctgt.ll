; TEST-ARGS: -tgt-is-asm

define half @src1(half %0) {
  %2 = call half @llvm.experimental.constrained.fadd.f16(half 0.000000e+00, half %0, metadata !"round.tonearest", metadata !"fpexcept.ignore")
  ret half %2
}

define half @tgt1(half %0) {
  %a5_3 = fadd half %0, 0.000000e+00
  ret half %a5_3
}

define i64 @src2(i64 %0) {
  %2 = alloca i8, i64 1, align 1
  %3 = getelementptr i8, ptr %2, i64 5
  %4 = getelementptr i8, ptr %3, i64 %0
  %5 = call i64 @llvm.objectsize.i64.p0(ptr %4, i1 false, i1 false, i1 true)
  ret i64 %5
}

define i64 @tgt2(i64 %0) {
  %a5_3 = add i64 %0, 5
  %a7_3 = sub i64 -4, %0
  %a8_3 = icmp ult i64 %a5_3, 2
  %a10_3 = select i1 %a8_3, i64 %a7_3, i64 0
  ret i64 %a10_3
}
