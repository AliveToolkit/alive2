; TEST-ARGS: -smt-to=8000
; https://bugs.llvm.org/show_bug.cgi?id=43948
@arr = local_unnamed_addr global [32 x i32] zeroinitializer, align 16
@var = global i32 zeroinitializer, align 8


define i32 @src() {
  %t0 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 0), align 16
  %t1 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 1), align 4
  %t2 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 2), align 8
  %t3 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 3), align 4
  %t4 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 4), align 16
  %t5 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 5), align 4
  %c01 = icmp sgt i32 %t0, %t1
  %s5 = select i1 %c01, i32 %t0, i32 %t1
  %c012 = icmp sgt i32 %s5, %t2
  %t8 = select i1 %c012, i32 %s5, i32 %t2
  %c0123 = icmp sgt i32 %t8, %t3
  %t11 = select i1 %c0123, i32 %t8, i32 %t3
  %EXTRA_USE = icmp sgt i32 %t11, %t4
  %t14 = select i1 %EXTRA_USE, i32 %t11, i32 %t4
  %c012345 = icmp sgt i32 %t14, %t5
  %t17 = select i1 %c012345, i32 %t14, i32 %t5
  %THREE_OR_FOUR = select i1 %EXTRA_USE, i32 3, i32 4
  store i32 %THREE_OR_FOUR, i32* @var, align 8
  ret i32 %t17
}

define i32 @tgt() {
  %1 = load <4 x i32>, <4 x i32>* bitcast ([32 x i32]* @arr to <4 x i32>*), align 16
  %t4 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 4), align 16
  %t5 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 5), align 4
  %rdx.shuf = shufflevector <4 x i32> %1, <4 x i32> undef, <4 x i32> <i32 2, i32 3, i32 undef, i32 undef>
  %rdx.minmax.cmp = icmp sgt <4 x i32> %1, %rdx.shuf
  %rdx.minmax.select = select <4 x i1> %rdx.minmax.cmp, <4 x i32> %1, <4 x i32> %rdx.shuf
  %rdx.shuf1 = shufflevector <4 x i32> %rdx.minmax.select, <4 x i32> undef, <4 x i32> <i32 1, i32 undef, i32 undef, i32 undef>
  %rdx.minmax.cmp2 = icmp sgt <4 x i32> %rdx.minmax.select, %rdx.shuf1
  %rdx.minmax.select3 = select <4 x i1> %rdx.minmax.cmp2, <4 x i32> %rdx.minmax.select, <4 x i32> %rdx.shuf1
  %2 = extractelement <4 x i32> %rdx.minmax.select3, i32 0
  %3 = icmp sgt i32 %2, %t4
  %4 = select i1 %3, i32 %2, i32 %t4
  %c012345 = icmp sgt i32 %4, %t5
  %t17 = select i1 %c012345, i32 %4, i32 %t5
  %THREE_OR_FOUR = select i1 undef, i32 3, i32 4
  store i32 %THREE_OR_FOUR, i32* @var, align 8
  ret i32 %t17
}

; ERROR: Mismatch in memory
