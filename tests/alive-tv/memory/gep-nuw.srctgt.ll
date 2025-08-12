define ptr @src1(ptr %p, i32 %idx) {
  %gep = getelementptr nuw i8, ptr %p, i32 %idx
  ret ptr %gep
}

define ptr @tgt1(ptr %p, i32 %idx) {
  %tmp = sext i32 %idx to i64
  %gep = getelementptr nuw i8, ptr %p, i64 %tmp
  ret ptr %gep
}

define ptr @src2(ptr %p, i32 %idx) {
  %gep = getelementptr nuw i8, ptr %p, i32 %idx
  ret ptr %gep
}

define ptr @tgt2(ptr %p, i32 %idx) {
  %tmp = sext i32 %idx to i64
  %t2 = zext i64 %tmp to i68
  %gep = getelementptr nuw i8, ptr %p, i68 %t2
  ret ptr %gep
}

define ptr @src3(ptr %p, i32 %idx) {
  %gep = getelementptr nusw i8, ptr %p, i32 %idx
  ret ptr %gep
}

define ptr @tgt3(ptr %p, i32 %idx) {
  %tmp = sext i32 %idx to i68
  %gep = getelementptr nusw i8, ptr %p, i68 %tmp
  ret ptr %gep
}
