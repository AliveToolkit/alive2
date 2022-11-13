declare { i32, i1 } @llvm.sadd.with.overflow.i32(i32, i32) nounwind memory(none)

define { i32, i1 } @src(i8 %a, i8 %b) {
  %aa = sext i8 %a to i32
  %bb = sext i8 %b to i32
  %x = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %aa, i32 %bb)
  ret { i32, i1 } %x
}


define { i32, i1 } @tgt(i8 %a, i8 %b) {
  %aa = sext i8 %a to i32
  %bb = sext i8 %b to i32
  %x = add nsw i32 %aa, %bb
  %tmp = insertvalue {i32, i1} {i32 undef, i1 false}, i32 %x, 0
  ret {i32, i1} %tmp
}
