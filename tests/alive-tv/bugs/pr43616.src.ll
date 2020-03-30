; https://bugs.llvm.org/show_bug.cgi?id=43616
; This miscompilation is detected by Alive2 in the past, but not now because
; changing a global variable into constant is not supported anymore.
declare {}* @llvm.invariant.start.p0i8(i64 %size, i8* nocapture %ptr)

;define void @test1(i8* %ptr) {
;  call {}* @llvm.invariant.start.p0i8(i64 4, i8* %ptr)
;  ret void
;}
; To reproduce the error, test1's fn body is not needed
declare void @test1(i8*)

@object1 = global i32 0
define void @ctor1() {
  store i32 -1, i32* @object1
  %A = bitcast i32* @object1 to i8*
  call void @test1(i8* %A)
  ret void
}

@llvm.global_ctors = appending constant
  [1 x { i32, void ()*, i8* }]
  [ { i32, void ()*, i8* } { i32 65535, void ()* @ctor1, i8* null } ]

; ERROR: Unsupported interprocedural transformation: global variable @object1 is const in target but not in source
