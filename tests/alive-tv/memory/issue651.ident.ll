; POR should consider poison-ness

declare i32* @g()

define void @f(i32** %p1, i32* %p2, i32** %p3, i32*) {
entry:
  load i32*, i32** %p1, align 8
  %cond1 = icmp eq i32* poison, %p2
  br i1 %cond1, label %if.then, label %exit
exit:
  ret void

if.then:
  %.1 = load i32, i32* %p2, align 8
  %cond2 = icmp eq i32 %.1, 0
  br i1 %cond2, label %if.then2, label %if.end2
if.then2:
  call i32* @g()
  br label %if.end2
if.end2:
  %i = load i32*, i32** %p3, align 8
  ret void
}
