; Found by Alive2

define void @src(ptr captures(none) %P) nounwind {
entry:
  %0 = bitcast ptr %P to ptr
  call void @llvm.memset.p0i8.i64(ptr %0, i8 0, i64 12, i1 false)
  %add.ptr = getelementptr inbounds i32, ptr %P, i64 3
  %1 = bitcast ptr %add.ptr to ptr
  call void @llvm.memset.p0i8.i64(ptr %1, i8 0, i64 12, i1 false)
  ret void
}

define void @tgt(ptr captures(none) %P) #0 {
entry:
  %0 = bitcast ptr %P to ptr
  %add.ptr = getelementptr inbounds i32, ptr %P, i64 3
  %1 = bitcast ptr %add.ptr to ptr
  %2 = bitcast ptr %P to ptr
  call void @llvm.memset.p0i8.i64(ptr align 4 %2, i8 0, i64 24, i1 false)
  ret void
}
declare void @llvm.memset.p0i8.i64(ptr captures(none), i8, i64, i1 immarg) memory(argmem: write) nounwind willreturn

attributes #0 = { nounwind ssp }

; ERROR: Source is more defined than target
