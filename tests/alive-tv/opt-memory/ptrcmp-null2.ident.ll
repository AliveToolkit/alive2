; TEST-ARGS: -dbg
define i1 @f(i8* %p) {
  %p2 = getelementptr inbounds i8, i8* %p, i64 0
  %c = icmp eq i8* null, %p2
  ret i1 %c
}

; CHECK: use_provenance
; CHECK: has_ptr2int_nonlocal: 0
