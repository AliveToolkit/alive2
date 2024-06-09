define i1 @src(i1 %1, i8 noundef range(i8 -32, -16) %2) {
  %5 = trunc nuw nsw i8 %2 to i4
  %6 = select i1 %1, i4 3, i4 %5
  %7 = trunc i4 %6 to i1
  ret i1 %7
}

define i1 @tgt(i1 %1, i8 noundef range(i8 -32, -16) %2) {
  %5 = trunc i8 %2 to i1
  %6 = or i1 %1, %5
  ret i1 %6
}
