; https://bugs.llvm.org/show_bug.cgi?id=43616
; This miscompilation is detected by Alive2 in the past, but not now because
; changing a global variable into constant is not supported anymore.
declare ptr @llvm.invariant.start.p0i8(i64 %size, ptr nocapture %ptr)

declare void @test1(ptr)

@object1 = global i32 0
define void @ctor1() {
  store i32 -1, ptr @object1
  call void @test1(ptr @object1)
  ret void
}

; ERROR: Unsupported interprocedural transformation: global variable @object1 is const in target but not in source
