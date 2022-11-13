@object1 = constant i32 -1

declare ptr @llvm.invariant.start.p0i8(i64 immarg, ptr nocapture) memory(argmem: readwrite) nounwind willreturn
declare void @test1(ptr)

define void @ctor1() {
  store i32 -1, ptr @object1
  call void @test1(ptr @object1)
  ret void
}
