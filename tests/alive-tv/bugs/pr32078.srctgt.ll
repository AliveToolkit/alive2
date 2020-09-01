; https://bugs.llvm.org/show_bug.cgi?id=32078
; ERROR: Target is more poisonous than source

define i1 @src(<2 x i32> %x, <2 x i32> %y) {
entry:
  %x0 = extractelement <2 x i32> %x, i32 0
  %y0 = extractelement <2 x i32> %y, i32 0
  %cmp0 = icmp eq i32 %x0, %y0
  br i1 %cmp0, label %if, label %endif

if:
  %x1 = extractelement <2 x i32> %x, i32 1
  %y1 = extractelement <2 x i32> %y, i32 1
  %cmp1 = icmp eq i32 %x1, %y1
  br label %endif

endif:
  %and_of_cmps = phi i1 [ false, %entry ], [ %cmp1, %if ]
  ret i1 %and_of_cmps
}

define i1 @tgt(<2 x i32> %x, <2 x i32> %y) {
  %vcmp = icmp eq <2 x i32> %x, %y
  %cmp0 = extractelement <2 x i1> %vcmp, i32 0
  %cmp1 = extractelement <2 x i1> %vcmp, i32 1
  %and_of_cmps = and i1 %cmp0, %cmp1
  ret i1 %and_of_cmps
}
