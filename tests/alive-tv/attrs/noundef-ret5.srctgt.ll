define i64 @src(i64 %x) {
  %rotl = call noundef i64 @llvm.fshl.i64(i64 %x, i64 %x, i64 3) 
  %masked = and i64 %rotl, -4
  ret i64 %masked
}

define noundef i64 @tgt(i64 %x) {
  %rotl = call noundef i64 @llvm.fshl.i64(i64 %x, i64 %x, i64 3) 
  %masked = and i64 %rotl, -4
  ret i64 %masked
}
