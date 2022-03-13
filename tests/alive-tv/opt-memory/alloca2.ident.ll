; TEST-ARGS: -dbg
; CHECK: bits_ptr_address: 6

define i1 @fn(i64* %arg) {
  %alloc = alloca i64
  %cmp = icmp eq i64* %arg, %alloc
  ret i1 %cmp
}
