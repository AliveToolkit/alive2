; Found by Alive2

define void @src(i32* nocapture %P) nounwind ssp {
entry:
  %0 = bitcast i32* %P to i8*
  tail call void @llvm.memset.p0i8.i64(i8* %0, i8 0, i64 12, i1 false)
  %add.ptr = getelementptr inbounds i32, i32* %P, i64 3
  %1 = bitcast i32* %add.ptr to i8*
  tail call void @llvm.memset.p0i8.i64(i8* %1, i8 0, i64 12, i1 false)
  ret void
}

define void @tgt(i32* nocapture %P) #0 {
entry:
  %0 = bitcast i32* %P to i8*
  %add.ptr = getelementptr inbounds i32, i32* %P, i64 3
  %1 = bitcast i32* %add.ptr to i8*
  %2 = bitcast i32* %P to i8*
  call void @llvm.memset.p0i8.i64(i8* align 4 %2, i8 0, i64 24, i1 false)
  ret void
}
; Function Attrs: argmemonly nounwind willreturn
declare void @llvm.memset.p0i8.i64(i8* nocapture writeonly, i8, i64, i1 immarg) #1

attributes #0 = { nounwind ssp }
attributes #1 = { argmemonly nounwind willreturn }

; ERROR: Source is more defined than target
