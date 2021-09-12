; TEST-ARGS: -dbg
define i1 @f(i8* %p) {
  %p2 = getelementptr inbounds i8, i8* %p, i64 0
  %p3 = getelementptr inbounds i8, i8* %p, i64 1
  %c = icmp eq i8* %p2, %p3
  ret i1 %c
}

; CHECK: use_provenance
; CHECK: observes_addresses: 0
