; TEST-ARGS: -disable-undef-input -smt-to=15000

define void @src(i32 %a, i32 %b, {i32, i1}* %p) {
  %mul = call { i32, i1 } @llvm.umul.with.overflow.i32(i32 %a, i32 %b)
  store {i32, i1} %mul, {i32, i1}* %p
  ret void
}

define void @tgt(i32 %a, i32 %b, {i32, i1}* %p) {
  %mul = call { i32, i1 } @llvm.smul.with.overflow.i32(i32 %a, i32 %b)
  store {i32, i1} %mul, {i32, i1}* %p
  ret void
}

; ERROR: Mismatch in memory
declare { i32, i1 } @llvm.umul.with.overflow.i32(i32, i32) nounwind readnone speculatable willreturn
declare { i32, i1 } @llvm.smul.with.overflow.i32(i32, i32) nounwind readnone speculatable willreturn
