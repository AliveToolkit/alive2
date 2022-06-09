; ERROR: The source program doesn't reach a return instruction

@glb = external global i8

define void @fn(ptr %b) {
entry:
  br label %for.cond

for.cond:
  %b.01 = phi ptr [ null, %entry ], [ %b, %for.cond ]
  %cmp = icmp ult ptr %b.01, @glb
  br i1 %cmp, label %for.cond, label %for.end

for.end:
  ret void
}
