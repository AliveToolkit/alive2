; https://bugs.llvm.org/show_bug.cgi?id=31243
target datalayout = "e-m:o-p:32:32-f64:32:64-f80:128-n8:16:32-S128"
target triple = "i386-apple-macosx10.11.0"

define i32 @src(i8 zeroext %v1, i8 zeroext %v2, i8 zeroext %v3, i8 zeroext %v4, ptr %ptr) {
entry:
  %xor1 = xor i8 0, %v1
  %xor2 = xor i8 0, %v2
  %xor3 = xor i8 0, %v3
  %xor4 = xor i8 0, %v4

  %add1 = add i8 %xor1, 30
  %add2 = add i8 %xor2, 29
  %add3 = add i8 %xor3, 48
  %add4 = add i8 %xor4, 67

  %shl1 = shl i8 %xor1, 1
  %shl2 = shl i8 %xor2, 1
  %shl3 = shl i8 %xor3, 1
  %shl4 = shl i8 %xor4, 1

  %and1 = and i8 %shl1, 60
  %and2 = and i8 %shl2, 58
  %and3 = and i8 %shl3, 96
  %and4 = and i8 %shl4, -122

  %sub1 = sub i8 %add1, %and1
  %sub2 = sub i8 %add2, %and2
  %sub3 = sub i8 %add3, %and3
  %sub4 = sub i8 %add4, %and4

  %tmp1 = xor i8 %sub1, 30
  %tmp2 = xor i8 %sub2, 29
  %tmp3 = xor i8 %sub3, 48
  %tmp4 = xor i8 %sub4, 67

  %xor11 = zext i8 %tmp1 to i32
  %xor12 = zext i8 %tmp2 to i32
  %xor13 = zext i8 %tmp3 to i32
  %xor14 = zext i8 %tmp4 to i32

  %arrayidx1 = getelementptr inbounds i8, ptr %ptr, i32 %xor11
  %arrayidx2 = getelementptr inbounds i8, ptr %ptr, i32 %xor12
  %arrayidx3 = getelementptr inbounds i8, ptr %ptr, i32 %xor13
  %arrayidx4 = getelementptr inbounds i8, ptr %ptr, i32 %xor14

  %load1 = load i8, ptr %arrayidx1, align 1
  %load2 = load i8, ptr %arrayidx2, align 1
  %load3 = load i8, ptr %arrayidx3, align 1
  %load4 = load i8, ptr %arrayidx4, align 1

  %conv1 = zext i8 %load1 to i32
  %conv2 = zext i8 %load2 to i32
  %conv3 = zext i8 %load3 to i32
  %conv4 = zext i8 %load4 to i32

  %sum1 = add nuw nsw i32 %conv1, %conv2
  %sum2 = add nuw nsw i32 %sum1, %conv3
  %sum3 = add nuw nsw i32 %sum2, %conv4
  ret i32 %sum3
}

define i32 @tgt(i8 zeroext %v1, i8 zeroext %v2, i8 zeroext %v3, i8 zeroext %v4, ptr %ptr) {
entry:
  %0 = insertelement <4 x i8> undef, i8 %v1, i32 0
  %1 = insertelement <4 x i8> %0, i8 %v2, i32 1
  %2 = insertelement <4 x i8> %1, i8 %v3, i32 2
  %3 = insertelement <4 x i8> %2, i8 %v4, i32 3
  %4 = xor <4 x i8> zeroinitializer, %3
  %5 = add <4 x i8> <i8 30, i8 29, i8 48, i8 67>, %4
  %6 = shl <4 x i8> %4, <i8 1, i8 1, i8 1, i8 1>
  %7 = and <4 x i8> <i8 60, i8 58, i8 96, i8 -122>, %6
  %8 = sub <4 x i8> %5, %7
  %9 = xor <4 x i8> <i8 30, i8 29, i8 48, i8 67>, %8
  %10 = zext <4 x i8> %9 to <4 x i32>
  %11 = trunc <4 x i32> %10 to <4 x i8>
  %12 = extractelement <4 x i8> %11, i32 0
  %13 = sext i8 %12 to i32
  %arrayidx1 = getelementptr inbounds i8, ptr %ptr, i32 %13
  %14 = extractelement <4 x i8> %11, i32 1
  %15 = sext i8 %14 to i32
  %arrayidx2 = getelementptr inbounds i8, ptr %ptr, i32 %15
  %16 = extractelement <4 x i8> %11, i32 2
  %17 = sext i8 %16 to i32
  %arrayidx3 = getelementptr inbounds i8, ptr %ptr, i32 %17
  %18 = extractelement <4 x i8> %11, i32 3
  %19 = sext i8 %18 to i32
  %arrayidx4 = getelementptr inbounds i8, ptr %ptr, i32 %19
  %load1 = load i8, ptr %arrayidx1, align 1
  %load2 = load i8, ptr %arrayidx2, align 1
  %load3 = load i8, ptr %arrayidx3, align 1
  %load4 = load i8, ptr %arrayidx4, align 1
  %conv1 = zext i8 %load1 to i32
  %conv2 = zext i8 %load2 to i32
  %conv3 = zext i8 %load3 to i32
  %conv4 = zext i8 %load4 to i32
  %sum1 = add nuw nsw i32 %conv1, %conv2
  %sum2 = add nuw nsw i32 %sum1, %conv3
  %sum3 = add nuw nsw i32 %sum2, %conv4
  ret i32 %sum3
}

; ERROR: Source is more defined than target
