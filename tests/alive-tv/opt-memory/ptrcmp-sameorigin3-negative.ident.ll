; TEST-ARGS: -dbg
define i1 @f(i8* %p, i64 %i, i64 %j) {
  %p2 = getelementptr inbounds i8, i8* %p, i64 %i
  %p3 = getelementptr i8, i8* %p, i64 %j
  %c = icmp ult i8* %p2, %p3
  ret i1 %c
}

; CHECK: has_ptr2int: 1
