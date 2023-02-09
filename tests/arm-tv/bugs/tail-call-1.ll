; XFAIL

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias nocapture writeonly, ptr noalias nocapture readonly, i64, i1 immarg) #0

; Function Attrs: nounwind
define void @test1(ptr %0, ptr %1, i64 %2) #1 {
  tail call void @llvm.memcpy.p0.p0.i64(ptr %0, ptr %1, i64 %2, i1 false)
  ret void
}

attributes #0 = { nocallback nofree nounwind willreturn memory(argmem: readwrite) }
attributes #1 = { nounwind }
