define i16 @src_smulh_ashr(i8 %a, i8 %b) {
  %ea = sext i8 %a to i16
  %eb = sext i8 %b to i16
  %mul = mul i16 %ea, %eb
  %shr = ashr i16 %mul, 8
  ret i16 %shr
}

define i16 @tgt_smulh_ashr(i8 %a, i8 %b) {
  %res = call i8 @llvm.smulh.i8(i8 %a, i8 %b)
  %ext = sext i8 %res to i16
  ret i16 %ext
}

define i16 @src_smulh_lshr(i8 %a, i8 %b) {
  %ea = sext i8 %a to i16
  %eb = sext i8 %b to i16
  %mul = mul i16 %ea, %eb
  %shr = lshr i16 %mul, 8
  ret i16 %shr
}

define i16 @tgt_smulh_lshr(i8 %a, i8 %b) {
  %res = call i8 @llvm.smulh.i8(i8 %a, i8 %b)
  %ext = zext i8 %res to i16
  ret i16 %ext
}

define i16 @src_umulh_ashr(i8 %a, i8 %b) {
  %ea = zext i8 %a to i16
  %eb = zext i8 %b to i16
  %mul = mul i16 %ea, %eb
  %shr = ashr i16 %mul, 8
  ret i16 %shr
}

define i16 @tgt_umulh_ashr(i8 %a, i8 %b) {
  %res = call i8 @llvm.umulh.i8(i8 %a, i8 %b)
  %ext = sext i8 %res to i16
  ret i16 %ext
}

define i16 @src_umulh_lshr(i8 %a, i8 %b) {
  %ea = zext i8 %a to i16
  %eb = zext i8 %b to i16
  %mul = mul i16 %ea, %eb
  %shr = lshr i16 %mul, 8
  ret i16 %shr
}

define i16 @tgt_umulh_lshr(i8 %a, i8 %b) {
  %res = call i8 @llvm.umulh.i8(i8 %a, i8 %b)
  %ext = zext i8 %res to i16
  ret i16 %ext
}
