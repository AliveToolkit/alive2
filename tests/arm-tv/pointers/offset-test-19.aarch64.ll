; ModuleID = 'foo.c'
source_filename = "foo.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

%struct.s = type { i64, i8, i8, i16, i16, i64 }

; Function Attrs: mustprogress nofree norecurse nosync nounwind ssp willreturn memory(argmem: readwrite) uwtable(sync)
define void @f(ptr nocapture noundef %p) local_unnamed_addr #0 {
entry:
  %f2 = getelementptr inbounds %struct.s, ptr %p, i64 0, i32 2
  store i8 0, ptr %f2, align 1
  store i64 0, ptr %p, align 8
  %f1 = getelementptr inbounds %struct.s, ptr %p, i64 0, i32 1
  store i8 0, ptr %f1, align 8
  %f3 = getelementptr inbounds %struct.s, ptr %p, i64 0, i32 3
  %0 = load i16, ptr %f3, align 2
  %conv7 = trunc i16 %0 to i8
  store i8 %conv7, ptr %f2, align 1
  %f5 = getelementptr inbounds %struct.s, ptr %p, i64 0, i32 5
  %1 = load i64, ptr %f5, align 8
  %conv9 = sext i16 %0 to i64
  %add10 = add nsw i64 %1, %conv9
  %conv12 = trunc i64 %add10 to i16
  %f4 = getelementptr inbounds %struct.s, ptr %p, i64 0, i32 4
  store i16 %conv12, ptr %f4, align 4
  %sext = shl i64 %add10, 32
  %conv13 = ashr exact i64 %sext, 32
  store i64 %conv13, ptr %f5, align 8
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
