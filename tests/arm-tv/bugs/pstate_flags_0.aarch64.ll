; TEST-ARGS:

; ModuleID = 'test-951506729.ll'
source_filename = "test-951506729.ll"

define i1 @test5(i32 %0, i32 %1) {
  %3 = icmp eq i32 %0, %1
  br i1 %3, label %4, label %7

4:                                                ; preds = %2
  %5 = udiv i32 %0, %1
  %6 = icmp ne i32 %5, %1
  ret i1 %6

7:                                                ; preds = %2
  %8 = icmp eq i32 %0, %1
  ret i1 %8
}
