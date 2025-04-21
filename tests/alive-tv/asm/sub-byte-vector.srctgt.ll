define void @src(ptr %0, ptr %1, ptr %2) {
  %4 = load <1 x i1>, ptr %1, align 1
  %5 = load <1 x i1>, ptr %2, align 1
  %6 = load <1 x i1>, ptr %0, align 1
  %7 = select <1 x i1> %6, <1 x i1> %4, <1 x i1> %5
  store <1 x i1> %7, ptr %0, align 1
  ret void
}

define void @tgt(ptr %0, ptr %1, ptr %2) {
  %a3_2 = load i8, ptr %0, align 1
  %a4_7.not = icmp eq i8 %a3_2, 0
  %a5_8.v = select i1 %a4_7.not, ptr %2, ptr %1
  %a6_2 = load i8, ptr %a5_8.v, align 1
  store i8 %a6_2, ptr %0, align 1
  ret void
}
