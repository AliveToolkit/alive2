; TEST-ARGS: -dbg
define i1 @f(ptr %p) {
  %p2 = getelementptr inbounds i8, ptr %p, i64 0
  %p3 = getelementptr inbounds i8, ptr %p, i64 1
  %c = icmp eq ptr %p2, %p3
  ret i1 %c
}

; CHECK: offsetonly
; CHECK: observes_addresses: 0
