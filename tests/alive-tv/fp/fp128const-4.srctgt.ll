define fp128 @src() {
  %x = uitofp i64 1 to fp128
  ret fp128 %x
}

define fp128 @tgt() {
  ret fp128 0xL00000000000000003FFF000000000000
}
