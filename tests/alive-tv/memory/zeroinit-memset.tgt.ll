; ModuleID = 'zeroinit-memset.src.ll'
source_filename = "zeroinit-memset.src.ll"
target datalayout = "e-i64:64-f80:128-n8:16:32:64"
target triple = "x86_64-unknown-linux-gnu"

%S = type { i8*, i8, i32 }
define void @destroysrc(%S* %src, %S* %dst) {
  %1 = load %S, %S* %src
  %2 = bitcast %S* %src to i8*
  call void @llvm.memset.p0i8.i64(i8* align 8 %2, i8 0, i64 16, i1 false)
  store %S %1, %S* %dst
  ret void
}
; Function Attrs: argmemonly nounwind willreturn
declare void @llvm.memset.p0i8.i64(i8* nocapture writeonly, i8, i64, i1 immarg) #0

attributes #0 = { argmemonly nounwind willreturn }
