; TEST-ARGS: -dbg
define i1 @f(ptr %p) {
  %p2 = getelementptr inbounds i8, ptr %p, i64 0
  %c = icmp eq ptr %p2, null
  ret i1 %c
}

; CHECK: observes_addresses: 1
