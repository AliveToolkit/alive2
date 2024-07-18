; TEST-ARGS: -dbg
define i1 @f(ptr %p, i64 %i, i64 %j) {
  %p2 = getelementptr inbounds [5 x i8], ptr %p, i8 1, i64 %i
  %p3 = getelementptr inbounds [5 x i8], ptr %p, i8 2, i64 %j
  %c = icmp ult ptr %p2, %p3
  ret i1 %c
}

; CHECK: offsetonly
; CHECK: observes_addresses: 0
