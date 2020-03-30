@object1 = constant i32 -1
@llvm.global_ctors = appending constant [0 x { i32, void ()*, i8* }] zeroinitializer

; Function Attrs: argmemonly nounwind willreturn
declare {}* @llvm.invariant.start.p0i8(i64 immarg, i8* nocapture) #0

;define void @test1(i8* %ptr) local_unnamed_addr {
;  %1 = call {}* @llvm.invariant.start.p0i8(i64 4, i8* %ptr)
;  ret void
;}
; To reproduce the error, test1's fn body is not needed
declare void @test1(i8*)

define void @ctor1() local_unnamed_addr {
  store i32 -1, i32* @object1
  %A = bitcast i32* @object1 to i8*
  call void @test1(i8* %A)
  ret void
}

attributes #0 = { argmemonly nounwind willreturn }
