; TEST-ARGS: -max-offset-in-bits=8
; ERROR: Value mismatch

define i4 @src(i8* %src, i8* %lower, i8* %upper, i8 %N, i8 %step) {
entry:
  %src.end = getelementptr inbounds i8, i8* %src, i8 %N
  %cmp.src.start = icmp ult i8* %src, %lower
  %cmp.src.end = icmp uge i8* %src.end, %upper
  %or.precond.0 = or i1 %cmp.src.start, %cmp.src.end
  br i1 %or.precond.0, label %trap.bb, label %step.check

trap.bb:
  ret i4 2

step.check:
  %step.pos = icmp uge i8 %step, 0
  %step.ult.N = icmp ult i8 %step, %N
  %and.step = and i1 %step.pos, %step.ult.N
  br i1 %and.step, label %ptr.check, label %exit

ptr.check:
  %src.step = getelementptr inbounds i8, i8* %src, i8 %step
  %cmp.step.start = icmp ult i8* %src.step, %lower
  %cmp.step.end = icmp uge i8* %src.step, %upper
  %or.check = or i1 %cmp.step.start, %cmp.step.end
  br i1 %or.check, label %trap.bb, label %exit

exit:
  ret i4 3
}

define i4 @tgt(i8* %src, i8* %lower, i8* %upper, i8 %N, i8 %step) {
entry:
  %src.end = getelementptr inbounds i8, i8* %src, i8 %N
  %cmp.src.start = icmp ult i8* %src, %lower
  %cmp.src.end = icmp uge i8* %src.end, %upper
  %or.precond.0 = or i1 %cmp.src.start, %cmp.src.end
  br i1 %or.precond.0, label %trap.bb, label %step.check

trap.bb:
  ret i4 2

step.check:
  %step.pos = icmp uge i8 %step, 0
  %step.ult.N = icmp ult i8 %step, %N
  %and.step = and i1 %step.pos, %step.ult.N
  br i1 %and.step, label %ptr.check, label %exit

ptr.check:
  %src.step = getelementptr inbounds i8, i8* %src, i8 %step
  %cmp.step.start = icmp ult i8* %src.step, %lower
  %cmp.step.end = icmp uge i8* %src.step, %upper
  %or.check = or i1 false, false
  br i1 %or.check, label %trap.bb, label %exit

exit:
  ret i4 3
}
