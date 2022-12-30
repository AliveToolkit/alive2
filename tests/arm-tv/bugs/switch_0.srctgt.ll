; TEST-ARGS: -backend-tv --disable-undef-input --disable-poison-input

; ModuleID = 'test-444577050.ll'
source_filename = "test-444577050.ll"

define i32 @foo(i32 %0) !prof !0 {
  switch i32 %0, label %7 [
    i32 8, label %2
    i32 -8826, label %3
    i32 18312, label %4
    i32 18568, label %5
    i32 129, label %6
  ], !prof !1

2:                                                ; preds = %1
  br label %8

3:                                                ; preds = %1
  br label %8

4:                                                ; preds = %1
  br label %8

5:                                                ; preds = %1
  br label %8

6:                                                ; preds = %1
  br label %8

7:                                                ; preds = %1
  br label %8

8:                                                ; preds = %7, %6, %5, %4, %3, %2
  %9 = phi i32 [ 0, %7 ], [ 5, %6 ], [ 4, %5 ], [ 3, %4 ], [ 2, %3 ], [ 1, %2 ]
  %10 = add i32 710479386, %9
  ret i32 %10
}

!0 = !{!"function_entry_count", i64 100000}
!1 = !{!"branch_weights", i32 50, i32 100, i32 200, i32 29500, i32 70000, i32 150}
