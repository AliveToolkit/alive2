define zeroext i8 @f(i61 %0, i64 zeroext %1) {
  %3 = freeze i64 %1
  %4 = icmp ne i64 %3, 0
  %5 = sext i1 %4 to i8
  ret i8 %5
}
