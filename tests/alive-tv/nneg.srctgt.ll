define i16 @src1(i8 %x) {
  %r = zext nneg i8 %x to i16
  ret i16 %r
}

define i16 @tgt1(i8 %x) {
  %r = sext i8 %x to i16
  ret i16 %r
}

define float @src2(i8 %x) {
  %r = uitofp nneg i8 %x to float
  ret float %r
}

define float @tgt2(i8 %x) {
  %r = sitofp i8 %x to float
  ret float %r
}
