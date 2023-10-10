; POR should consider poison-ness

declare ptr @g()

define void @f(ptr %p1, ptr %p2, ptr %p3, ptr) {
entry:
  load ptr, ptr %p1, align 8
  %cond1 = icmp eq ptr poison, %p2
  br i1 %cond1, label %if.then, label %exit
exit:
  ret void

if.then:
  %.1 = load i32, ptr %p2, align 8
  %cond2 = icmp eq i32 %.1, 0
  br i1 %cond2, label %if.then2, label %if.end2
if.then2:
  call ptr @g()
  br label %if.end2
if.end2:
  %i = load ptr, ptr %p3, align 8
  ret void
}
