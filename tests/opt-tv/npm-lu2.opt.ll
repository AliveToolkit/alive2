; TEST-ARGS: -passes='unswitch<nontrivial>' -tv-src-unroll=2 -tv-tgt-unroll=2

define i32 @test(i1 %cond) {
entry:
  br label %loop
loop:
  %i = phi i32 [0, %entry], [%i.next, %loop.end]
  br i1 %cond, label %bb1, label %bb2
bb1:
  call void @f()
  br label %loop.end
bb2:
  call void @g()
  br label %loop.end
loop.end:
  %i.next = add i32 %i, 1
  %cmp = icmp eq i32 %i.next, 2
  br i1 %cmp, label %exit, label %loop
exit:
  ret i32 0
}

declare void @f()
declare void @g()
