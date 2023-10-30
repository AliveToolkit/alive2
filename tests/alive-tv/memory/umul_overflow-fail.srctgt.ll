define void @src(i16 %a, i16 %b, ptr %p) {
  %mul = call { i16, i1 } @llvm.umul.with.overflow.i16(i16 %a, i16 %b)
  store {i16, i1} %mul, ptr %p
  ret void
}

define void @tgt(i16 %a, i16 %b, ptr %p) {
  %mul = call { i16, i1 } @llvm.smul.with.overflow.i16(i16 %a, i16 %b)
  store {i16, i1} %mul, ptr %p
  ret void
}

; ERROR: Mismatch in memory
declare { i16, i1 } @llvm.umul.with.overflow.i16(i16, i16) nounwind memory(none) willreturn
declare { i16, i1 } @llvm.smul.with.overflow.i16(i16, i16) nounwind memory(none) willreturn
