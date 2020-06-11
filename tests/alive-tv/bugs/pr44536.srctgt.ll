; Found by Alive2

@arr_i32 = global [32 x i32] zeroinitializer, align 16
declare i32 @foobar(i32)

define void @src(i32 %val) {
entry:
  %0 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr_i32, i64 0, i64 0), align 16
  %1 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr_i32, i64 0, i64 1), align 4
  %add = add nsw i32 %1, %0
  %2 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr_i32, i64 0, i64 2), align 8
  %add.1 = add nsw i32 %2, %add
  %3 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr_i32, i64 0, i64 3), align 4
  %add.2 = add nsw i32 %3, %add.1
  %4 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr_i32, i64 0, i64 4), align 16
  %add.3 = add nsw i32 %4, %add.2
  %5 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr_i32, i64 0, i64 5), align 4
  %add.4 = add nsw i32 %5, %add.3
  %6 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr_i32, i64 0, i64 6), align 8
  %add.5 = add nsw i32 %6, %add.4
  %7 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr_i32, i64 0, i64 7), align 4
  %add.6 = add nsw i32 %7, %add.5
  %res = call i32 @foobar(i32 %add.6)
  ret void
}

define void @tgt(i32 %val) #0 {
entry:
  %0 = load <8 x i32>, <8 x i32>* bitcast ([32 x i32]* @arr_i32 to <8 x i32>*), align 16
  %rdx.shuf = shufflevector <8 x i32> %0, <8 x i32> undef, <8 x i32> <i32 4, i32 5, i32 6, i32 7, i32 undef, i32 undef, i32 undef, i32 undef>
  %bin.rdx = add nsw <8 x i32> %0, %rdx.shuf
  %rdx.shuf1 = shufflevector <8 x i32> %bin.rdx, <8 x i32> undef, <8 x i32> <i32 2, i32 3, i32 undef, i32 undef, i32 undef, i32 undef, i32 undef, i32 undef>
  %bin.rdx2 = add nsw <8 x i32> %bin.rdx, %rdx.shuf1
  %rdx.shuf3 = shufflevector <8 x i32> %bin.rdx2, <8 x i32> undef, <8 x i32> <i32 1, i32 undef, i32 undef, i32 undef, i32 undef, i32 undef, i32 undef, i32 undef>
  %bin.rdx4 = add nsw <8 x i32> %bin.rdx2, %rdx.shuf3
  %1 = extractelement <8 x i32> %bin.rdx4, i32 0
  %res = call i32 @foobar(i32 %1)
  ret void
}

attributes #0 = { "target-cpu"="corei7-avx" }

; ERROR: Source is more defined than target
