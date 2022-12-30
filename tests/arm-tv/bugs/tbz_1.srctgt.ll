; TEST-ARGS: -backend-tv --disable-undef-input --disable-poison-input

define i32 @separate_predecessors-tgt(i1 %0, i1 %1, i1 %2, i1 %3, i1 %4, i32 %5, i32 %6) {
7:
  br i1 %0, label %8, label %9

8:
  %a = phi i32 [ %c, %9 ], [ %5, %7 ]
  br i1 %3, label %10, label %9

9:
  %b = phi i32 [ %a, %8 ], [ %6, %7 ]
  %c = add i32 %b, 1
  br i1 %4, label %10, label %8

10:
  %d = phi i32 [ %a, %8 ], [ %c, %9 ]
  ret i32 %d
}