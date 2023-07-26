; TEST-ARGS: -dbg
; CHECK: bits_ptr_address: 6

define i1 @fn(ptr %arg) {
  %alloc = alloca i64
  %cmp = icmp eq ptr %arg, %alloc
  ret i1 %cmp
}
