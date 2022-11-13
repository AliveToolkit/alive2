define void @src(i32 %a, i32 %b) {
  %mul = call { i32, i1 } @llvm.umul.with.overflow.i32(i32 %a, i32 %b)
  ret void
}

define void @tgt(i32 %a, i32 %b) {
  %mul = call { i32, i1 } @llvm.umul.with.overflow.i32(i32 %a, i32 %b)
  ret void
}

declare { i32, i1 } @llvm.umul.with.overflow.i32(i32, i32) nounwind memory(none) willreturn
