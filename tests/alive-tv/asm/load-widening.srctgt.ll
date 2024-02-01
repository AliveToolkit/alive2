; TEST-ARGS: -tgt-is-asm

define void @src(ptr %0, ptr %1) {
  %3 = load <6 x i32>, ptr %0, align 16
  %4 = insertelement <6 x i32> %3, i32 0, i64 4
  load <8 x i32>, ptr %0, align 16
  store <6 x i32> %4, ptr %1, align 16
  ret void
}

define void @tgt(ptr %0, ptr %1) {
  %a2_1 = load i128, ptr %0, align 1
  %3 = getelementptr i8, ptr %0, i64 16
  %a2_213 = load <4 x i32>, ptr %3, align 1
  %a3_4 = insertelement <4 x i32> %a2_213, i32 0, i64 0
  store i128 %a2_1, ptr %1, align 1
  %bc = bitcast <4 x i32> %a3_4 to <2 x i64>
  %a5_2 = extractelement <2 x i64> %bc, i64 0
  %a5_3 = getelementptr i8, ptr %1, i64 16
  store i64 %a5_2, ptr %a5_3, align 1
  ret void
}
