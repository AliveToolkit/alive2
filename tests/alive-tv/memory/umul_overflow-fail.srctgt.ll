define void @src(i32 %a, i32 %b, ptr %p) {
  %mul = call { i32, i1 } @llvm.umul.with.overflow.i32(i32 %a, i32 %b)
  store {i32, i1} %mul, ptr %p
  ret void
}

define void @tgt(i32 %a, i32 %b, ptr %p) {
  %mul = call { i32, i1 } @llvm.smul.with.overflow.i32(i32 %a, i32 %b)
  store {i32, i1} %mul, ptr %p
  ret void
}

; ERROR: Mismatch in memory
declare { i32, i1 } @llvm.umul.with.overflow.i32(i32, i32) nounwind memory(none) willreturn
declare { i32, i1 } @llvm.smul.with.overflow.i32(i32, i32) nounwind memory(none) willreturn
