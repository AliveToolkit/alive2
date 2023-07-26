; Found by Alive2
; https://bugs.llvm.org/show_bug.cgi?id=46591

target triple = "x86_64--"

define void @src(i32 %v0, i32 %v1, ptr %src, ptr %dst) {
bb:
  %tmp = add nuw i32 %v0, -1
  %tmp1 = add nuw i32 %v1, %tmp
  %tmp2 = zext i32 %tmp1 to i64
  %tmp3 = getelementptr inbounds i8, ptr %src, i64 %tmp2
  %tmp4 = load i8, ptr %tmp3, align 1
  %tmp5 = add nuw i32 %v1, %v0
  %tmp6 = zext i32 %tmp5 to i64
  %tmp7 = getelementptr inbounds i8, ptr %src, i64 %tmp6
  %tmp8 = load i8, ptr %tmp7, align 1
  %tmp9 = add nuw i32 %v0, 1
  %tmp10 = add nuw i32 %v1, %tmp9
  %tmp11 = zext i32 %tmp10 to i64
  %tmp12 = getelementptr inbounds i8, ptr %src, i64 %tmp11
  %tmp13 = load i8, ptr %tmp12, align 1
  %tmp14 = add nuw i32 %v0, 2
  %tmp15 = add nuw i32 %v1, %tmp14
  %tmp16 = zext i32 %tmp15 to i64
  %tmp17 = getelementptr inbounds i8, ptr %src, i64 %tmp16
  %tmp18 = load i8, ptr %tmp17, align 1
  %tmp19 = insertelement <4 x i8> poison, i8 %tmp4, i32 0
  %tmp20 = insertelement <4 x i8> %tmp19, i8 %tmp8, i32 1
  %tmp21 = insertelement <4 x i8> %tmp20, i8 %tmp13, i32 2
  %tmp22 = insertelement <4 x i8> %tmp21, i8 %tmp18, i32 3
  store <4 x i8> %tmp22, ptr %dst
  ret void
}

define void @tgt(i32 %v0, i32 %v1, ptr %src, ptr %dst) {
bb:
  %tmp = add nuw i32 %v0, -1
  %tmp1 = add nuw i32 %v1, %tmp
  %tmp2 = zext i32 %tmp1 to i64
  %tmp3 = getelementptr inbounds i8, ptr %src, i64 %tmp2
  %0 = bitcast ptr %tmp3 to ptr
  %1 = load <4 x i8>, ptr %0, align 1
  %tmp41 = extractelement <4 x i8> %1, i32 0
  %tmp82 = extractelement <4 x i8> %1, i32 1
  %tmp133 = extractelement <4 x i8> %1, i32 2
  %tmp184 = extractelement <4 x i8> %1, i32 3
  %tmp19 = insertelement <4 x i8> poison, i8 %tmp41, i32 0
  %tmp20 = insertelement <4 x i8> %tmp19, i8 %tmp82, i32 1
  %tmp21 = insertelement <4 x i8> %tmp20, i8 %tmp133, i32 2
  %tmp22 = insertelement <4 x i8> %tmp21, i8 %tmp184, i32 3
  store <4 x i8> %tmp22, ptr %dst, align 4
  ret void
}

; ERROR: Source is more defined than target
