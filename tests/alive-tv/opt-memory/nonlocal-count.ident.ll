; TEST-ARGS: -dbg
; CHECK: num_nonlocals_src: 6

define void @f(ptr %p, i1 %c) {
  br i1 %c, label %then, label %else

then:
  load ptr, ptr %p
  %p01 = getelementptr ptr, ptr %p, i32 3
  load ptr, ptr %p01
  br label %exit

else:
  load ptr, ptr %p
  %p2 = getelementptr ptr, ptr %p, i32 1
  load ptr, ptr %p2
  %p3 = getelementptr ptr, ptr %p, i32 2
  load ptr, ptr %p3
  %p4 = getelementptr ptr, ptr %p, i32 4
  load ptr, ptr %p4
  br label %exit

exit:
  load ptr, ptr %p
  ret void
}
