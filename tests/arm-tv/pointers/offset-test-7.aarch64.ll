; ModuleID = 'foo.c'
source_filename = "foo.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

%struct.s = type { i8, i64, i8, i64, i16, i16 }

; Function Attrs: mustprogress nofree norecurse nosync nounwind ssp willreturn memory(argmem: readwrite) uwtable(sync)
define void @f(ptr nocapture noundef %p) local_unnamed_addr #0 {
entry:
  %0 = load i8, ptr %p, align 8
  %conv = zext i8 %0 to i32
  %f5 = getelementptr inbounds %struct.s, ptr %p, i64 0, i32 5
  %1 = load i16, ptr %f5, align 2
  %conv3 = zext i16 %1 to i32
  %f1 = getelementptr inbounds %struct.s, ptr %p, i64 0, i32 1
  %2 = load i64, ptr %f1, align 8
  %3 = trunc i64 %2 to i32
  %factor = shl nuw nsw i32 %conv, 1
  %conv7 = add nuw nsw i32 %factor, %conv3
  %add10 = add i32 %conv7, %3
  %conv11 = trunc i32 %add10 to i16
  store i16 %conv11, ptr %f5, align 2
  %conv13 = trunc i32 %add10 to i8
  store i8 %conv13, ptr %p, align 8
  ret void
}

attributes #0 = { mustprogress nofree norecurse nosync nounwind ssp willreturn memory(argmem: readwrite) uwtable(sync) "frame-pointer"="non-leaf" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="apple-m1" "target-features"="+aes,+crc,+dotprod,+fp-armv8,+fp16fml,+fullfp16,+lse,+neon,+ras,+rcpc,+rdm,+sha2,+sha3,+v8.1a,+v8.2a,+v8.3a,+v8.4a,+v8.5a,+v8a,+zcm,+zcz" }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 2, !"SDK Version", [2 x i32] [i32 13, i32 1]}
!1 = !{i32 1, !"wchar_size", i32 4}
!2 = !{i32 8, !"PIC Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 1}
!4 = !{i32 7, !"frame-pointer", i32 1}
!5 = !{!"clang version 17.0.0 (git@github.com:llvm/llvm-project.git c32022ad260aa1e6f485c5a6820fa9973f3b108e)"}
