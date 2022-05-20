; TEST-ARGS: -src-unroll=1

@c = external global i16, align 1

define void @src() {
entry:
  br label %for.cond

for.cond:
  %0 = phi i16 [ 0, %entry ], [ ptrtoint (i16* @c to i16), %if.end ], [ 0, %for.cond1 ]
  br i1 0, label %for.cond1, label %for.end8

for.cond1:
  br i1 0, label %for.cond, label %if.end

if.end:
  br i1 0, label %for.cond, label %for.cond1

for.end8:
  ret void
}

define void @tgt() {
  ret void
}
