; TEST-ARGS: -dbg
define i1 @f(ptr %p, i64 %i, i64 %j) {
  %p2 = getelementptr inbounds i8, ptr %p, i64 %i
  %p3 = getelementptr i8, ptr %p, i64 %j
  %c = icmp ult ptr %p2, %p3
  ret i1 %c
}

; CHECK: has_ptr2int: 1
